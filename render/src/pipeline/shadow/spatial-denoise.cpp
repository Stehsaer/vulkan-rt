#include "render/pipeline/shadow/spatial-denoise.hpp"
#include "common/number-literals.hpp"
#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "render/interface/camera.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/shadow.hpp"
#include "shader/shadow/spatial-denoise.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/base-level.hpp"
#include "vulkan/util/trivial-descriptor-set.hpp"

#include <cstdint>
#include <expected>
#include <glm/ext/vector_uint3_sized.hpp>
#include <libassert/assert.hpp>
#include <ranges>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render::shadow
{
	std::expected<SpatialDenoisePipeline, Error> SpatialDenoisePipeline::create(
		const vulkan::Context& context
	) noexcept
	{
		/*===== Descriptor Set Layout =====*/

		auto input_layout_result = vulkan::trivset::Layout<Input>::create(context);
		if (!input_layout_result)
			return input_layout_result.error().forward("Create descriptor set layout failed");
		auto input_layout = std::move(*input_layout_result);

		/*===== Pipeline =====*/

		auto pipeline_result = Pipeline::create(context, input_layout, shader::shadow::spatial_denoise);
		if (!pipeline_result) return pipeline_result.error().forward("Create pipeline failed");
		auto pipeline = std::move(*pipeline_result);

		/*===== Sampler =====*/

		const auto sampler_info = vk::SamplerCreateInfo{
			.magFilter = vk::Filter::eNearest,
			.minFilter = vk::Filter::eNearest,
			.mipmapMode = vk::SamplerMipmapMode::eNearest,
			.addressModeU = vk::SamplerAddressMode::eClampToEdge,
			.addressModeV = vk::SamplerAddressMode::eClampToEdge,
			.addressModeW = vk::SamplerAddressMode::eClampToEdge,
			.mipLodBias = 0,
			.anisotropyEnable = vk::False,
			.maxAnisotropy = 0,
			.compareEnable = vk::False,
			.minLod = 0,
			.maxLod = 0,
			.unnormalizedCoordinates = vk::True
		};

		auto sampler_result = context.device.createSampler(sampler_info);
		if (!sampler_result) return Error::from(sampler_result);
		auto sampler = std::move(*sampler_result);

		return SpatialDenoisePipeline(std::move(input_layout), std::move(pipeline), std::move(sampler));
	}

	std::expected<std::vector<SpatialDenoisePipeline::ResourceSet>, Error> SpatialDenoisePipeline::
		create_resource_sets(const vulkan::Context& context, uint32_t count) const noexcept
	{
		auto sets_result = input_layout.create_sets(context, count * FILTER_PASSES);
		if (!sets_result) return sets_result.error().forward("Create sets failed");

		return std::views::zip_transform(
				   CTOR_LAMBDA(ResourceSet),
				   *sets_result
					   | std::views::as_rvalue
					   | std::views::chunk(FILTER_PASSES)
					   | std::views::transform([](auto&& chunk) {
							 return std::vector(std::from_range, chunk);
						 }),
				   std::views::repeat(*sampler, count)
			   )
			| std::ranges::to<std::vector>();
	}

	void SpatialDenoisePipeline::denoise(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource_set
	) const noexcept
	{
		DEBUG_ASSERT(resource_set.resource.has_value());
		DEBUG_ASSERT(resource_set.sets.size() == FILTER_PASSES);

		for (const auto [iter, set] : std::views::enumerate(resource_set.sets))
		{
			const auto output_image = iter % 2 == 0 ? resource_set->denoise_bob : resource_set->denoise_alice;

			const auto output_image_pre_barrier = vk::ImageMemoryBarrier2{
				.srcStageMask = {},
				.srcAccessMask = {},
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eGeneral,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = output_image.image,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			};

			const auto output_image_post_barrier = vk::ImageMemoryBarrier2{
				.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.dstStageMask =
					vk::PipelineStageFlagBits2::eComputeShader | vk::PipelineStageFlagBits2::eFragmentShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
				.oldLayout = vk::ImageLayout::eGeneral,
				.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = output_image.image,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			};

			command_buffer.pipelineBarrier2(
				vk::DependencyInfo().setImageMemoryBarriers(output_image_pre_barrier)
			);

			pipeline.dispatch(
				command_buffer,
				*set,
				PushConstant{
					.half_size = resource_set->half_extent,
					.stride = 1_u32 << iter,
				},
				glm::u32vec3(resource_set->half_extent, 1)
			);

			command_buffer.pipelineBarrier2(
				vk::DependencyInfo().setImageMemoryBarriers(output_image_post_barrier)
			);
		}
	}

	void SpatialDenoisePipeline::ResourceSet::update(
		const vulkan::Context& context,
		vulkan::ElementBufferRef<Camera> camera,
		HalfDeferredAttachment::View half_gbuffer,
		ShadowAttachment::View shadow
	) noexcept
	{
		using namespace vulkan::trivset;

		DEBUG_ASSERT(half_gbuffer.half_extent == shadow.half_extent);
		DEBUG_ASSERT(half_gbuffer.full_extent == shadow.full_extent);
		DEBUG_ASSERT(sets.size() == FILTER_PASSES);

		for (const auto [iter, set] : sets | std::views::as_const | std::views::enumerate)
		{
			const vulkan::AttachmentView input_tex =
				iter % 2 == 0 ? shadow.denoise_alice : shadow.denoise_bob;
			const vulkan::AttachmentView output_tex =
				iter % 2 == 0 ? shadow.denoise_bob : shadow.denoise_alice;

			const auto input = Input{
				.input_tex = input_tex + sampler,
				.output_tex = output_tex,
				.depth_tex = half_gbuffer.depth + sampler,
				.normal_tex = half_gbuffer.smooth_normal + sampler,
				.camera = camera
			};

			set.update(context, input);
		}

		resource = Resource{
			.half_extent = shadow.half_extent,
			.denoise_alice = shadow.denoise_alice,
			.denoise_bob = shadow.denoise_bob,
		};
	}
}

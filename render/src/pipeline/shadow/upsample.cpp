#include "render/pipeline/shadow/upsample.hpp"
#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "render/interface/camera.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/shadow.hpp"
#include "shader/shadow/upsample/gen.hpp"
#include "shader/shadow/upsample/mask.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/base-level.hpp"
#include "vulkan/util/sampler.hpp"
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
	std::expected<UpsamplePipeline, Error> UpsamplePipeline::create(const vulkan::Context& context) noexcept
	{
		auto mask_set_layout_result = vulkan::trivset::Layout<MaskInput>::create(context);
		if (!mask_set_layout_result)
			return mask_set_layout_result.error().forward("Create descriptor set layout for mask failed");
		auto mask_set_layout = std::move(*mask_set_layout_result);

		auto gen_set_layout_result = vulkan::trivset::Layout<GenInput>::create(context);
		if (!gen_set_layout_result)
			return gen_set_layout_result.error().forward("Create descriptor set layout for gen failed");
		auto gen_set_layout = std::move(*gen_set_layout_result);

		auto mask_pipeline_result =
			MaskPipeline::create(context, mask_set_layout, shader::shadow::upsample::mask);
		if (!mask_pipeline_result) return mask_pipeline_result.error().forward("Create mask pipeline failed");
		auto mask_pipeline = std::move(*mask_pipeline_result);

		auto gen_pipeline_result =
			GenPipeline::create(context, gen_set_layout, shader::shadow::upsample::gen);
		if (!gen_pipeline_result) return gen_pipeline_result.error().forward("Create gen pipeline failed");
		auto gen_pipeline = std::move(*gen_pipeline_result);

		auto sampler_result = context.device.createSampler(
			vulkan::SamplerFilter::Nearest
			+ vk::SamplerAddressMode::eClampToEdge
			+ vulkan::SamplerUnnormalized
		);
		if (!sampler_result) return Error::from(sampler_result);
		auto sampler = std::move(*sampler_result);

		return UpsamplePipeline(
			std::move(mask_set_layout),
			std::move(gen_set_layout),
			std::move(mask_pipeline),
			std::move(gen_pipeline),
			std::move(sampler)
		);
	}

	std::expected<std::vector<UpsamplePipeline::ResourceSet>, Error> UpsamplePipeline::create_resource_sets(
		const vulkan::Context& context,
		uint32_t count
	) const noexcept
	{
		auto mask_sets_result = mask_set_layout.create_sets(context, count);
		auto gen_sets_result = gen_set_layout.create_sets(context, count);

		if (!mask_sets_result) return mask_sets_result.error().forward("Create mask sets failed");
		if (!gen_sets_result) return gen_sets_result.error().forward("Create gen sets failed");

		auto mask_sets = std::move(*mask_sets_result);
		auto gen_sets = std::move(*gen_sets_result);

		return std::views::zip_transform(
				   CTOR_LAMBDA(ResourceSet),
				   std::views::as_rvalue(mask_sets),
				   std::views::as_rvalue(gen_sets),
				   std::views::repeat(*sampler, count)
			   )
			| std::ranges::to<std::vector>();
	}

	void UpsamplePipeline::upsample(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource_set
	) const noexcept
	{
		DEBUG_ASSERT(resource_set.resource.has_value());

		const auto resolution = Resolution{
			.half = resource_set->half_extent,
			.full = resource_set->full_extent,
		};

		{
			const auto pre_barrier = vk::ImageMemoryBarrier2{
				.srcStageMask = {},
				.srcAccessMask = {},
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eGeneral,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = resource_set->bitmask.image,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			};

			const auto post_barrier = vk::ImageMemoryBarrier2{
				.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
				.oldLayout = vk::ImageLayout::eGeneral,
				.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = resource_set->bitmask.image,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			};

			command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_barrier));
			mask_pipeline.dispatch(
				command_buffer,
				*resource_set.mask_set,
				resolution,
				glm::u32vec3(resource_set->half_extent, 1)
			);
			command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(post_barrier));
		}

		{
			const auto pre_barrier = vk::ImageMemoryBarrier2{
				.srcStageMask = {},
				.srcAccessMask = {},
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eGeneral,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = resource_set->visibility.image,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			};

			const auto post_barrier = vk::ImageMemoryBarrier2{
				.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
				.oldLayout = vk::ImageLayout::eGeneral,
				.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = resource_set->visibility.image,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			};

			command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_barrier));
			gen_pipeline.dispatch(
				command_buffer,
				*resource_set.gen_set,
				resolution,
				glm::u32vec3(resource_set->half_extent, 1)
			);
			command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(post_barrier));
		}
	}

	void UpsamplePipeline::ResourceSet::update(
		const vulkan::Context& context,
		DeferredAttachment::View gbuffer,
		ShadowAttachment::View attachment,
		vulkan::ElementBufferRef<Camera> camera
	) noexcept
	{
		using namespace vulkan::trivset;

		DEBUG_ASSERT(attachment.full_extent == gbuffer.extent);

		const auto mask_input = MaskInput{
			.full_depth_tex = gbuffer.depth + sampler,
			.full_normal_tex = gbuffer.geom_normal + sampler,
			.camera = camera,
			.visibility_mask = attachment.upsample_bitmask
		};

		const auto gen_input = GenInput{
			.half_shadow = attachment.denoise_bob + sampler,
			.full_shadow = attachment.visibility,
			.visibility_mask = attachment.upsample_bitmask + sampler,
		};

		mask_set.update(context, mask_input);
		gen_set.update(context, gen_input);

		resource = Resource{
			.half_extent = attachment.half_extent,
			.full_extent = attachment.full_extent,
			.bitmask = attachment.upsample_bitmask,
			.visibility = attachment.visibility,
		};
	}
}

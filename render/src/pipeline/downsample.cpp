#include "render/pipeline/downsample.hpp"
#include "common/util/array.hpp"
#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "render/resource/deferred.hpp"
#include "shader/downsample.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/base-level.hpp"
#include "vulkan/util/trivial-descriptor-set.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <glm/ext/vector_uint3_sized.hpp>
#include <libassert/assert.hpp>
#include <ranges>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	namespace
	{
		std::expected<vk::raii::Sampler, Error> create_sampler(const vulkan::Context& context) noexcept
		{
			auto sampler_result = context.device.createSampler({
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
				.unnormalizedCoordinates = vk::True,
			});
			if (!sampler_result) return Error::from(sampler_result);
			auto sampler = std::move(*sampler_result);

			return sampler;
		}
	}

	std::expected<DownsamplePipeline, Error> DownsamplePipeline::create(
		const vulkan::Context& context
	) noexcept
	{
		auto descriptor_set_layout_result = vulkan::trivset::Layout<Input>::create(context);
		if (!descriptor_set_layout_result)
			return descriptor_set_layout_result.error().forward("Create descriptor set layout failed");
		auto descriptor_set_layout = std::move(*descriptor_set_layout_result);

		auto pipeline_result = Pipeline::create(context, descriptor_set_layout, shader::downsample);
		if (!pipeline_result) return pipeline_result.error().forward("Create downsample pipeline failed");
		auto pipeline = std::move(*pipeline_result);

		auto sampler_result = create_sampler(context);
		if (!sampler_result) return sampler_result.error().forward("Create downsample sampler failed");
		auto sampler = std::move(*sampler_result);

		return DownsamplePipeline(
			std::move(descriptor_set_layout),
			std::move(pipeline),
			std::move(sampler)
		);
	}

	std::expected<std::vector<DownsamplePipeline::ResourceSet>, Error>
	DownsamplePipeline::create_resource_sets(const vulkan::Context& context, uint32_t count) const noexcept
	{
		auto sets_result = descriptor_set_layout.create_sets(context, count);
		if (!sets_result) return sets_result.error().forward("Create descriptor sets failed");
		auto sets = std::move(*sets_result);

		return std::vector(
			std::from_range,
			std::views::zip_transform(
				CTOR_LAMBDA(ResourceSet),
				std::views::as_rvalue(sets),
				std::views::repeat(static_cast<vk::Sampler>(texture_sampler))
			)
		);
	}

	void DownsamplePipeline::downsample(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource_set
	) const noexcept
	{
		DEBUG_ASSERT(resource_set.attachment.has_value());

		const auto attachments = std::to_array({
			resource_set.attachment->albedo,
			resource_set.attachment->normal,
			resource_set.attachment->geom_normal,
			resource_set.attachment->smooth_normal,
			resource_set.attachment->pbr,
			resource_set.attachment->depth,
		});

		/*===== Pre-Synchronize =====*/

		const auto pre_barriers =
			attachments | util::map_array([](vulkan::AttachmentView attachment) {
				return vk::ImageMemoryBarrier2{
					.srcStageMask = {},
					.srcAccessMask = {},
					.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
					.dstAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
					.oldLayout = vk::ImageLayout::eUndefined,
					.newLayout = vk::ImageLayout::eGeneral,
					.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
					.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
					.image = attachment.image,
					.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
				};
			});

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_barriers));

		/*===== Downsample =====*/

		pipeline.dispatch(
			command_buffer,
			*resource_set.descriptor_set,
			{resource_set.attachment->half_extent},
			glm::u32vec3(resource_set.attachment->half_extent, 1)
		);

		/*===== Post-Synchronize =====*/

		const auto post_barriers =
			attachments | util::map_array([](vulkan::AttachmentView attachment) {
				return vk::ImageMemoryBarrier2{
					.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
					.srcAccessMask = vk::AccessFlagBits2::eShaderStorageWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader
						| vk::PipelineStageFlagBits2::eComputeShader
						| vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
					.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
					.oldLayout = vk::ImageLayout::eGeneral,
					.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
					.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
					.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
					.image = attachment.image,
					.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
				};
			});

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(post_barriers));
	}

	void DownsamplePipeline::ResourceSet::update(
		const vulkan::Context& context,
		DeferredAttachment::View deferred,
		HalfDeferredAttachment::View halfres_deferred
	) noexcept
	{
		using namespace vulkan::trivset;

		const auto input = Input{
			.full_albedo_tex = deferred.albedo + texture_sampler,
			.full_normal_tex = deferred.normal + texture_sampler,
			.full_geom_normal_tex = deferred.geom_normal + texture_sampler,
			.full_smooth_normal_tex = deferred.smooth_normal + texture_sampler,
			.full_pbr_tex = deferred.pbr + texture_sampler,
			.full_depth_tex = deferred.depth + texture_sampler,

			.half_albedo_tex = halfres_deferred.albedo,
			.half_normal_tex = halfres_deferred.normal,
			.half_geom_normal_tex = halfres_deferred.geom_normal,
			.half_smooth_normal_tex = halfres_deferred.smooth_normal,
			.half_pbr_tex = halfres_deferred.pbr,
			.half_depth_tex = halfres_deferred.depth,
		};

		descriptor_set.update(context, input);

		attachment = halfres_deferred;
	}
}

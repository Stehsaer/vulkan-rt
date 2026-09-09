#include "render/pipeline/shadow/spatial-variance.hpp"
#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "render/interface/camera.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/shadow.hpp"
#include "shader/shadow/spatial-variance/compute.hpp"
#include "shader/shadow/spatial-variance/filter.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/base-level.hpp"
#include "vulkan/util/sampler.hpp"
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

namespace render::shadow
{
	std::expected<SpatialVariancePipeline, Error> SpatialVariancePipeline::create(
		const vulkan::Context& context
	) noexcept
	{
		/*===== Descriptor Set Layout =====*/

		auto compute_set_layout_result = vulkan::trivset::Layout<ComputeInput>::create(context);
		if (!compute_set_layout_result)
			return compute_set_layout_result.error().forward("Create compute descriptor set layout failed");
		auto compute_set_layout = std::move(*compute_set_layout_result);

		auto filter_set_layout_result = vulkan::trivset::Layout<FilterInput>::create(context);
		if (!filter_set_layout_result)
			return filter_set_layout_result.error().forward("Create filter descriptor set layout failed");
		auto filter_set_layout = std::move(*filter_set_layout_result);

		/*===== Pipeline Layout =====*/

		auto compute_pipeline_result =
			ComputePipeline::create(context, compute_set_layout, shader::shadow::spatial_variance::compute);
		auto filter_pipeline_result =
			FilterPipeline::create(context, filter_set_layout, shader::shadow::spatial_variance::filter);

		auto compute_pipeline = std::move(*compute_pipeline_result);
		auto filter_pipeline = std::move(*filter_pipeline_result);

		/*===== Sampler =====*/

		auto sampler_result = context.device.createSampler(
			vulkan::SamplerFilter::Nearest
			+ vk::SamplerAddressMode::eClampToEdge
			+ vulkan::SamplerUnnormalized
		);
		if (!sampler_result) return Error::from(sampler_result);
		auto sampler = std::move(*sampler_result);

		return SpatialVariancePipeline(
			std::move(compute_set_layout),
			std::move(compute_pipeline),
			std::move(filter_set_layout),
			std::move(filter_pipeline),
			std::move(sampler)
		);
	}

	std::expected<std::vector<SpatialVariancePipeline::ResourceSet>, Error> SpatialVariancePipeline::
		create_resource_sets(const vulkan::Context& context, uint32_t count) const noexcept
	{
		auto compute_sets_result = compute_set_layout.create_sets(context, count);
		if (!compute_sets_result)
			return compute_sets_result.error().forward("Create compute descriptor sets failed");
		auto compute_sets = std::move(*compute_sets_result);

		auto filter_sets_result = filter_set_layout.create_sets(context, count);
		if (!filter_sets_result)
			return filter_sets_result.error().forward("Create filter descriptor sets failed");
		auto filter_sets = std::move(*filter_sets_result);

		return std::views::zip_transform(
				   CTOR_LAMBDA(ResourceSet),
				   std::views::as_rvalue(compute_sets),
				   std::views::as_rvalue(filter_sets),
				   std::views::repeat(*sampler, count)
			   )
			| std::ranges::to<std::vector>();
	}

	void SpatialVariancePipeline::compute(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource_set
	) const noexcept
	{
		DEBUG_ASSERT(resource_set.resource.has_value());

		/*===== Pre-barriers =====*/
		{
			const auto mean_tex_barrier = vk::ImageMemoryBarrier2{
				.srcStageMask = {},
				.srcAccessMask = {},
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eGeneral,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = resource_set->spatial_mean.image,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			};

			const auto stddev_tex_barrier = vk::ImageMemoryBarrier2{
				.srcStageMask = {},
				.srcAccessMask = {},
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eGeneral,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = resource_set->spatial_stddev.image,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			};

			const auto barriers = std::to_array({mean_tex_barrier, stddev_tex_barrier});
			command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barriers));
		}

		compute_pipeline.dispatch(
			command_buffer,
			resource_set.compute_set,
			{
				.half = resource_set->half_extent,
				.full = resource_set->full_extent,
			},
			glm::u32vec3(resource_set->half_extent, 1)
		);

		/*===== Post-barrier =====*/
		{
			const auto mean_tex_barrier = vk::ImageMemoryBarrier2{
				.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
				.oldLayout = vk::ImageLayout::eGeneral,
				.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = resource_set->spatial_mean.image,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			};

			const auto stddev_tex_barrier = vk::ImageMemoryBarrier2{
				.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
				.oldLayout = vk::ImageLayout::eGeneral,
				.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = resource_set->spatial_stddev.image,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			};

			const auto barriers = std::to_array({mean_tex_barrier, stddev_tex_barrier});
			command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barriers));
		}
	}

	void SpatialVariancePipeline::filter(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource_set
	) const noexcept
	{
		DEBUG_ASSERT(resource_set.resource.has_value());

		const auto pre_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = {},
			.srcAccessMask = {},
			.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
			.dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eGeneral,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = resource_set->filtered_spatial_stddev.image,
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
			.image = resource_set->filtered_spatial_stddev.image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		};

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_barrier));
		filter_pipeline.dispatch(
			command_buffer,
			resource_set.filter_set,
			{.half = resource_set->half_extent, .full = resource_set->full_extent},
			glm::u32vec3(resource_set->half_extent, 1)
		);
		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(post_barrier));
	}

	void SpatialVariancePipeline::generate(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource_set
	) const noexcept
	{
		compute(command_buffer, resource_set);
		filter(command_buffer, resource_set);
	}

	void SpatialVariancePipeline::ResourceSet::update(
		const vulkan::Context& context,
		HalfDeferredAttachment::View half_deferred,
		ShadowAttachment::View shadow,
		vulkan::ElementBufferRef<Camera> camera
	) noexcept
	{
		DEBUG_ASSERT(half_deferred.half_extent == shadow.half_extent);

		using namespace vulkan::trivset;

		const auto compute_input = ComputeInput{
			.input_tex = shadow.init_sample + sampler,
			.mean_tex = shadow.spatial_mean,
			.stddev_tex = shadow.spatial_stddev,
			.depth_tex = half_deferred.depth + sampler,
			.normal_tex = half_deferred.geom_normal + sampler,
			.camera = camera,
		};

		const auto filter_input = FilterInput{
			.input_stddev_tex = shadow.spatial_stddev + sampler,
			.output_stddev_tex = shadow.filtered_spatial_stddev,
			.depth_tex = half_deferred.depth + sampler,
			.normal_tex = half_deferred.geom_normal + sampler,
			.camera = camera,
		};

		compute_set.update(context, compute_input);
		filter_set.update(context, filter_input);

		resource = Resource{
			.half_extent = shadow.half_extent,
			.full_extent = shadow.full_extent,
			.spatial_mean = shadow.spatial_mean,
			.spatial_stddev = shadow.spatial_stddev,
			.filtered_spatial_stddev = shadow.filtered_spatial_stddev,
		};
	}
}

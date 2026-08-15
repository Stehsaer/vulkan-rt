#include "render/pipeline/shadow/spatial-variance.hpp"
#include "common/number-literals.hpp"
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
#include "vulkan/util/shader.hpp"
#include "vulkan/util/trivial-descriptor-set.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
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

		const auto push_constant_range = vk::PushConstantRange{
			.stageFlags = vk::ShaderStageFlagBits::eCompute,
			.offset = 0,
			.size = sizeof(Extent),
		};

		const auto compute_set_layouts = std::to_array({*compute_set_layout});
		const auto filter_set_layouts = std::to_array({*filter_set_layout});

		auto compute_pipeline_layout_result = context.device.createPipelineLayout(
			vk::PipelineLayoutCreateInfo()
				.setPushConstantRanges(push_constant_range)
				.setSetLayouts(compute_set_layouts)
		);
		if (!compute_pipeline_layout_result) return Error::from(compute_pipeline_layout_result);
		auto compute_pipeline_layout = std::move(*compute_pipeline_layout_result);

		auto filter_pipeline_layout_result = context.device.createPipelineLayout(
			vk::PipelineLayoutCreateInfo()
				.setPushConstantRanges(push_constant_range)
				.setSetLayouts(filter_set_layouts)
		);
		if (!filter_pipeline_layout_result) return Error::from(filter_pipeline_layout_result);
		auto filter_pipeline_layout = std::move(*filter_pipeline_layout_result);

		/*===== Shader =====*/

		auto compute_shader_module_result =
			vulkan::create_shader(context.device, shader::shadow::spatial_variance::compute);
		if (!compute_shader_module_result)
			return compute_shader_module_result.error().forward("Create compute shader module failed");
		auto compute_shader_module = std::move(*compute_shader_module_result);

		auto filter_shader_module_result =
			vulkan::create_shader(context.device, shader::shadow::spatial_variance::filter);
		if (!filter_shader_module_result)
			return filter_shader_module_result.error().forward("Create filter shader module failed");
		auto filter_shader_module = std::move(*filter_shader_module_result);

		const auto compute_shader_info = vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eCompute,
			.module = compute_shader_module,
			.pName = "main",
		};

		const auto filter_shader_info = vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eCompute,
			.module = filter_shader_module,
			.pName = "main",
		};

		/*===== Pipeline =====*/

		auto compute_pipeline_result = context.device.createComputePipeline(
			nullptr,
			vk::ComputePipelineCreateInfo{
				.stage = compute_shader_info,
				.layout = compute_pipeline_layout,
			}
		);
		if (!compute_pipeline_result) return Error::from(compute_pipeline_result);
		auto compute_pipeline = std::move(*compute_pipeline_result);

		auto filter_pipeline_result = context.device.createComputePipeline(
			nullptr,
			vk::ComputePipelineCreateInfo{
				.stage = filter_shader_info,
				.layout = filter_pipeline_layout,
			}
		);
		if (!filter_pipeline_result) return Error::from(filter_pipeline_result);
		auto filter_pipeline = std::move(*filter_pipeline_result);

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

		return SpatialVariancePipeline(
			std::move(compute_set_layout),
			std::move(compute_pipeline_layout),
			std::move(compute_pipeline),
			std::move(filter_set_layout),
			std::move(filter_pipeline_layout),
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
		const auto dispatch_size = (resource_set->half_extent + BLOCK_SIZE - 1_u32) / BLOCK_SIZE;

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

		/*===== Compute =====*/
		{
			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, compute_pipeline);
			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				compute_pipeline_layout,
				0,
				*resource_set.compute_set,
				{}
			);
			command_buffer.pushConstants<Extent>(
				compute_pipeline_layout,
				vk::ShaderStageFlagBits::eCompute,
				0,
				Extent{
					.half = resource_set->half_extent,
					.full = resource_set->full_extent,
				}
			);
			command_buffer.dispatch(dispatch_size.x, dispatch_size.y, 1);
		}

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
		const auto dispatch_size = (resource_set->half_extent + BLOCK_SIZE - 1_u32) / BLOCK_SIZE;

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

		command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, filter_pipeline);
		command_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eCompute,
			filter_pipeline_layout,
			0,
			*resource_set.filter_set,
			{}
		);
		command_buffer.pushConstants<Extent>(
			filter_pipeline_layout,
			vk::ShaderStageFlagBits::eCompute,
			0,
			Extent{
				.half = resource_set->half_extent,
				.full = resource_set->full_extent,
			}
		);
		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_barrier));
		command_buffer.dispatch(dispatch_size.x, dispatch_size.y, 1);
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

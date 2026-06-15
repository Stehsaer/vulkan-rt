#include "render/pipeline/motion-vector.hpp"
#include "common/number-literals.hpp"
#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "render/interface/camera.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/motion-vector.hpp"
#include "shader/motion-vector.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/base-level.hpp"
#include "vulkan/numeric/pool-size.hpp"
#include "vulkan/util/descriptor-set-layout.hpp"
#include "vulkan/util/shader.hpp"

#include <cstdint>
#include <expected>
#include <libassert/assert.hpp>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	using SetLayout = vulkan::MonoDescriptorSetLayout<
		// depth_tex
		vulkan::MonoDescriptorSetSlot<
			vk::DescriptorType::eCombinedImageSampler,
			vk::ShaderStageFlagBits::eCompute
		>,
		// prev_depth_tex
		vulkan::MonoDescriptorSetSlot<
			vk::DescriptorType::eCombinedImageSampler,
			vk::ShaderStageFlagBits::eCompute
		>,
		// motion_vector_tex
		vulkan::MonoDescriptorSetSlot<vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute>,
		// camera
		vulkan::MonoDescriptorSetSlot<vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute>
	>;

	std::expected<MotionVectorPipeline, Error> MotionVectorPipeline::create(
		const vulkan::Context& context
	) noexcept
	{
		auto set_layout_result = SetLayout::create_descriptor_set_layout(context);
		if (!set_layout_result)
			return set_layout_result.error().forward("Create descriptor set layout failed");
		auto set_layout = std::move(*set_layout_result);

		const auto push_constant_range = vk::PushConstantRange{
			.stageFlags = vk::ShaderStageFlagBits::eCompute,
			.offset = 0,
			.size = sizeof(PushConstant),
		};
		const auto set_layout_raw = *set_layout;

		auto pipeline_layout_result = context.device.createPipelineLayout(
			vk::PipelineLayoutCreateInfo()
				.setPushConstantRanges(push_constant_range)
				.setSetLayouts(set_layout_raw)
		);
		if (!pipeline_layout_result) return Error::from(pipeline_layout_result);
		auto pipeline_layout = std::move(*pipeline_layout_result);

		auto shader_result = vulkan::create_shader(context.device, shader::motion_vector);
		if (!shader_result) return shader_result.error();
		auto shader = std::move(*shader_result);

		const auto shader_stage = vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eCompute,
			.module = shader,
			.pName = "main",
		};

		const auto create_info =
			vk::ComputePipelineCreateInfo().setLayout(pipeline_layout).setStage(shader_stage);
		auto pipeline_result = context.device.createComputePipeline(nullptr, create_info);
		if (!pipeline_result) return Error::from(pipeline_result);
		auto pipeline = std::move(*pipeline_result);

		constexpr auto sampler_create_info = vk::SamplerCreateInfo{
			.magFilter = vk::Filter::eLinear,
			.minFilter = vk::Filter::eLinear,
			.mipmapMode = vk::SamplerMipmapMode::eNearest,

			.addressModeU = vk::SamplerAddressMode::eClampToEdge,
			.addressModeV = vk::SamplerAddressMode::eClampToEdge,
			.addressModeW = vk::SamplerAddressMode::eClampToEdge,
			.mipLodBias = 0.0f,
			.minLod = 0.0f,
			.maxLod = 0.0f,
			.unnormalizedCoordinates = vk::True
		};
		auto sampler_result = context.device.createSampler(sampler_create_info);
		if (!sampler_result) return Error::from(sampler_result);
		auto sampler = std::move(*sampler_result);

		return MotionVectorPipeline(
			std::move(set_layout),
			std::move(pipeline_layout),
			std::move(pipeline),
			std::move(sampler)
		);
	}

	std::expected<std::vector<MotionVectorPipeline::ResourceSet>, Error>
	MotionVectorPipeline::create_resource_sets(const vulkan::Context& context, uint32_t count) const noexcept
	{
		static constexpr auto BINDINGS = SetLayout::get_bindings();
		const auto pool_sizes = vulkan::calc_pool_sizes(BINDINGS, count);

		auto pool_result = context.device.createDescriptorPool(
			vk::DescriptorPoolCreateInfo()
				.setPoolSizes(pool_sizes)
				.setMaxSets(count)
				.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
		);
		if (!pool_result) return Error::from(pool_result);
		auto pool = std::make_shared<vk::raii::DescriptorPool>(std::move(*pool_result));

		const auto layouts = std::vector(count, *set_layout);
		auto set_result = context.device.allocateDescriptorSets(
			vk::DescriptorSetAllocateInfo().setSetLayouts(layouts).setDescriptorPool(*pool)
		);
		if (!set_result) return Error::from(set_result);
		auto sets = std::move(*set_result);

		return std::views::zip_transform(
				   CTOR_LAMBDA(ResourceSet),
				   std::views::repeat(pool, count),
				   std::views::as_rvalue(sets),
				   std::views::repeat(*sampler, count)
			   )
			| std::ranges::to<std::vector>();
	}

	void MotionVectorPipeline::compute(
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
			.image = resource_set->motion_vector.image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor),
		};

		const auto post_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
			.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader
				| vk::PipelineStageFlagBits2::eRayTracingShaderKHR
				| vk::PipelineStageFlagBits2::eFragmentShader,
			.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
			.oldLayout = vk::ImageLayout::eGeneral,
			.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = resource_set->motion_vector.image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor),
		};

		const auto groups = (resource_set->half_size + BLOCK_SIZE - 1_u32) / BLOCK_SIZE;

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_barrier));
		command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
		command_buffer
			.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, {resource_set.set}, {});
		command_buffer.pushConstants<PushConstant>(
			pipeline_layout,
			vk::ShaderStageFlagBits::eCompute,
			0,
			PushConstant{.half_size = resource_set->half_size, .full_size = resource_set->full_size}
		);
		command_buffer.dispatch(groups.x, groups.y, 1);
		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(post_barrier));
	}

	void MotionVectorPipeline::ResourceSet::update(
		const vulkan::Context& context,
		MotionVectorAttachment::View attachment,
		HalfDeferredAttachment::View gbuffer,
		HalfDeferredAttachment::View prev_gbuffer,
		vulkan::ElementBufferRef<Camera> camera
	) noexcept
	{
		DEBUG_ASSERT(attachment.half_extent == gbuffer.half_extent);
		DEBUG_ASSERT(attachment.half_extent == prev_gbuffer.half_extent);
		DEBUG_ASSERT(attachment.full_extent == gbuffer.full_extent);
		DEBUG_ASSERT(attachment.full_extent == prev_gbuffer.full_extent);

		const auto depth_tex = vk::DescriptorImageInfo{
			.sampler = sampler,
			.imageView = gbuffer.depth.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		const auto prev_depth_tex = vk::DescriptorImageInfo{
			.sampler = sampler,
			.imageView = prev_gbuffer.depth.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		const auto motion_vector_tex = vk::DescriptorImageInfo{
			.sampler = nullptr,
			.imageView = attachment.motion_vector.view,
			.imageLayout = vk::ImageLayout::eGeneral,
		};

		const auto camera_buf = vk::DescriptorBufferInfo{
			.buffer = camera,
			.offset = 0,
			.range = vk::WholeSize,
		};

		const auto write_sets =
			SetLayout::get_write_infos(set, depth_tex, prev_depth_tex, motion_vector_tex, camera_buf);

		context.device.updateDescriptorSets(write_sets, {});

		resource = Resource{
			.full_size = attachment->full_extent,
			.half_size = attachment->half_extent,
			.motion_vector = attachment->motion_vector,
		};
	}
}

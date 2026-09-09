#include "render/pipeline/motion-vector.hpp"
#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "render/interface/camera.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/motion-vector.hpp"
#include "shader/motion-vector.hpp"
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

namespace render
{
	std::expected<MotionVectorPipeline, Error> MotionVectorPipeline::create(
		const vulkan::Context& context
	) noexcept
	{
		auto set_layout_result = vulkan::trivset::Layout<Input>::create(context);
		if (!set_layout_result)
			return set_layout_result.error().forward("Create descriptor set layout failed");
		auto set_layout = std::move(*set_layout_result);

		auto pipeline_result = Pipeline::create(context, set_layout, shader::motion_vector);
		if (!pipeline_result) return pipeline_result.error().forward("Create pipeline failed");
		auto pipeline = std::move(*pipeline_result);

		auto sampler_result = context.device.createSampler(
			vulkan::SamplerFilter::LinearMipmapNearest
			+ vk::SamplerAddressMode::eClampToEdge
			+ vulkan::SamplerUnnormalized
		);
		if (!sampler_result) return Error::from(sampler_result);
		auto sampler = std::move(*sampler_result);

		return MotionVectorPipeline(std::move(set_layout), std::move(pipeline), std::move(sampler));
	}

	std::expected<std::vector<MotionVectorPipeline::ResourceSet>, Error>
	MotionVectorPipeline::create_resource_sets(const vulkan::Context& context, uint32_t count) const noexcept
	{
		auto sets_result = set_layout.create_sets(context, count);
		if (!sets_result) return sets_result.error().forward("Create descriptor sets failed");
		auto sets = std::move(*sets_result);

		return std::views::zip_transform(
				   CTOR_LAMBDA(ResourceSet),
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

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_barrier));
		pipeline.dispatch(
			command_buffer,
			*resource_set.set,
			PushConstant{.half_size = resource_set->half_size, .full_size = resource_set->full_size},
			glm::u32vec3(resource_set->half_size, 1)
		);
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
		using namespace vulkan::trivset;

		DEBUG_ASSERT(attachment.half_extent == gbuffer.half_extent);
		DEBUG_ASSERT(attachment.half_extent == prev_gbuffer.half_extent);
		DEBUG_ASSERT(attachment.full_extent == gbuffer.full_extent);
		DEBUG_ASSERT(attachment.full_extent == prev_gbuffer.full_extent);

		const auto input = Input{
			.depth_tex = gbuffer.depth + sampler,
			.prev_depth_tex = prev_gbuffer.depth + sampler,
			.motion_vector_tex = attachment.motion_vector,
			.camera = camera
		};

		set.update(context, input);

		resource = Resource{
			.full_size = attachment->full_extent,
			.half_size = attachment->half_extent,
			.motion_vector = attachment->motion_vector,
		};
	}
}

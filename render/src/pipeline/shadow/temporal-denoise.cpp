#include "render/pipeline/shadow/temporal-denoise.hpp"
#include "common/number-literals.hpp"
#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "render/resource/motion-vector.hpp"
#include "render/resource/shadow.hpp"
#include "shader/shadow/temporal-denoise.hpp"
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
	std::expected<TemporalDenoisePipeline, Error> TemporalDenoisePipeline::create(
		const vulkan::Context& context
	) noexcept
	{
		auto set_layout_result = vulkan::trivset::Layout<Input>::create(context);
		if (!set_layout_result)
			return set_layout_result.error().forward("Create descriptor set layout failed");
		auto set_layout = std::move(*set_layout_result);

		const auto push_constant_range = vk::PushConstantRange{
			.stageFlags = vk::ShaderStageFlagBits::eCompute,
			.offset = 0,
			.size = sizeof(glm::u32vec2),
		};
		const auto set_layouts = std::to_array({*set_layout});

		auto pipeline_layout_result = context.device.createPipelineLayout(
			vk::PipelineLayoutCreateInfo()
				.setPushConstantRanges(push_constant_range)
				.setSetLayouts(set_layouts)
		);
		if (!pipeline_layout_result) return Error::from(pipeline_layout_result);
		auto pipeline_layout = std::move(*pipeline_layout_result);

		auto shader_module_result = vulkan::create_shader(context.device, shader::shadow::temporal_denoise);
		if (!shader_module_result)
			return shader_module_result.error().forward("Create compute shader module failed");
		auto shader_module = std::move(*shader_module_result);

		const auto shader_info = vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eCompute,
			.module = shader_module,
			.pName = "main",
		};

		auto pipeline_result = context.device.createComputePipeline(
			nullptr,
			vk::ComputePipelineCreateInfo{
				.stage = shader_info,
				.layout = pipeline_layout,
			}
		);
		if (!pipeline_result) return Error::from(pipeline_result);
		auto pipeline = std::move(*pipeline_result);

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

		return TemporalDenoisePipeline(
			std::move(set_layout),
			std::move(pipeline_layout),
			std::move(pipeline),
			std::move(sampler)
		);
	}

	std::expected<std::vector<TemporalDenoisePipeline::ResourceSet>, Error> TemporalDenoisePipeline::
		create_resource_sets(const vulkan::Context& context, uint32_t count) const noexcept
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

	void TemporalDenoisePipeline::denoise(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource_set
	) const noexcept
	{
		DEBUG_ASSERT(resource_set.resource.has_value());
		const auto dispatch_size = (resource_set->half_extent + BLOCK_SIZE - 1_u32) / BLOCK_SIZE;

		/*===== Pre-barriers =====*/
		{
			const auto history_barrier = vk::ImageMemoryBarrier2{
				.srcStageMask = {},
				.srcAccessMask = {},
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eGeneral,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = resource_set->curr_history.image,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			};

			const auto denoise_alice_barrier = vk::ImageMemoryBarrier2{
				.srcStageMask = {},
				.srcAccessMask = {},
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.oldLayout = vk::ImageLayout::eUndefined,
				.newLayout = vk::ImageLayout::eGeneral,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = resource_set->denoise_alice.image,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			};

			const auto barriers = std::to_array({history_barrier, denoise_alice_barrier});
			command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barriers));
		}

		command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
		command_buffer
			.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, *resource_set.set, {});
		command_buffer.pushConstants<glm::u32vec2>(
			pipeline_layout,
			vk::ShaderStageFlagBits::eCompute,
			0,
			resource_set->half_extent
		);
		command_buffer.dispatch(dispatch_size.x, dispatch_size.y, 1);

		/*===== Post-barrier =====*/
		{
			const auto history_barrier = vk::ImageMemoryBarrier2{
				.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
				.oldLayout = vk::ImageLayout::eGeneral,
				.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = resource_set->curr_history.image,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			};

			const auto denoise_alice_barrier = vk::ImageMemoryBarrier2{
				.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
				.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
				.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
				.oldLayout = vk::ImageLayout::eGeneral,
				.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.image = resource_set->denoise_alice.image,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			};

			const auto barriers = std::to_array({history_barrier, denoise_alice_barrier});
			command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(barriers));
		}
	}

	void TemporalDenoisePipeline::ResourceSet::update(
		const vulkan::Context& context,
		ShadowAttachment::View shadow,
		ShadowAttachment::View prev_shadow,
		MotionVectorAttachment::View motion_vector
	) noexcept
	{
		using namespace vulkan::trivset;

		DEBUG_ASSERT(shadow.half_extent == prev_shadow.half_extent);
		DEBUG_ASSERT(shadow.half_extent == motion_vector.half_extent);

		const auto input = Input{
			.prev_history_tex = prev_shadow.history + sampler,
			.curr_shadow_tex = shadow.init_sample + sampler,
			.mean_tex = shadow.spatial_mean + sampler,
			.stddev_tex = shadow.filtered_spatial_stddev + sampler,
			.motion_vector_tex = motion_vector.motion_vector + sampler,
			.denoise_tex = shadow.denoise_alice,
			.curr_history_tex = shadow.history
		};

		set.update(context, input);

		resource = Resource{
			.half_extent = shadow.half_extent,
			.curr_history = shadow.history,
			.denoise_alice = shadow.denoise_alice
		};
	}
}

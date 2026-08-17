#include "render/pipeline/shadow/upsample.hpp"
#include "common/number-literals.hpp"
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
#include "vulkan/util/shader.hpp"
#include "vulkan/util/trivial-descriptor-set.hpp"

#include <array>
#include <cstdint>
#include <expected>
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

		const auto push_constant_range = vk::PushConstantRange{
			.stageFlags = vk::ShaderStageFlagBits::eCompute,
			.offset = 0,
			.size = sizeof(Resolution),
		};

		const auto mask_set_layouts = std::to_array({*mask_set_layout});
		const auto mask_pipeline_layout_create_info =
			vk::PipelineLayoutCreateInfo()
				.setSetLayouts(mask_set_layouts)
				.setPushConstantRanges(push_constant_range);
		auto mask_pipeline_layout_result =
			context.device.createPipelineLayout(mask_pipeline_layout_create_info);
		if (!mask_pipeline_layout_result) return Error::from(mask_pipeline_layout_result);
		auto mask_pipeline_layout = std::move(*mask_pipeline_layout_result);

		auto mask_shader_module_result =
			vulkan::create_shader(context.device, shader::shadow::upsample::mask);
		if (!mask_shader_module_result)
			return mask_shader_module_result.error().forward("Create shader module for mask failed");
		auto mask_shader_module = std::move(*mask_shader_module_result);

		const auto mask_shader_module_info = vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eCompute,
			.module = mask_shader_module,
			.pName = "main",
		};
		const auto mask_pipeline_create_info = vk::ComputePipelineCreateInfo{
			.stage = mask_shader_module_info,
			.layout = mask_pipeline_layout,
		};
		auto mask_pipeline_result = context.device.createComputePipeline(nullptr, mask_pipeline_create_info);
		if (!mask_pipeline_result) return Error::from(mask_pipeline_result);
		auto mask_pipeline = std::move(*mask_pipeline_result);

		const auto gen_set_layouts = std::to_array({*gen_set_layout});
		const auto gen_pipeline_layout_create_info =
			vk::PipelineLayoutCreateInfo()
				.setSetLayouts(gen_set_layouts)
				.setPushConstantRanges(push_constant_range);
		auto gen_pipeline_layout_result =
			context.device.createPipelineLayout(gen_pipeline_layout_create_info);
		if (!gen_pipeline_layout_result) return Error::from(gen_pipeline_layout_result);
		auto gen_pipeline_layout = std::move(*gen_pipeline_layout_result);

		auto gen_shader_module_result = vulkan::create_shader(context.device, shader::shadow::upsample::gen);
		if (!gen_shader_module_result)
			return gen_shader_module_result.error().forward("Create shader module for gen failed");
		auto gen_shader_module = std::move(*gen_shader_module_result);

		const auto gen_shader_module_info = vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eCompute,
			.module = gen_shader_module,
			.pName = "main",
		};
		const auto gen_pipeline_create_info = vk::ComputePipelineCreateInfo{
			.stage = gen_shader_module_info,
			.layout = gen_pipeline_layout,
		};
		auto gen_pipeline_result = context.device.createComputePipeline(nullptr, gen_pipeline_create_info);
		if (!gen_pipeline_result) return Error::from(gen_pipeline_result);
		auto gen_pipeline = std::move(*gen_pipeline_result);

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

		return UpsamplePipeline(
			std::move(mask_set_layout),
			std::move(gen_set_layout),
			std::move(mask_pipeline_layout),
			std::move(gen_pipeline_layout),
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

		const auto dispatch_size = (resource_set->half_extent + BLOCK_SIZE - 1_u32) / BLOCK_SIZE;
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

			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, mask_pipeline);
			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				mask_pipeline_layout,
				0,
				*resource_set.mask_set,
				{}
			);
			command_buffer.pushConstants<Resolution>(
				mask_pipeline_layout,
				vk::ShaderStageFlagBits::eCompute,
				0,
				resolution
			);

			command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_barrier));
			command_buffer.dispatch(dispatch_size.x, dispatch_size.y, 1);
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

			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, gen_pipeline);
			command_buffer.bindDescriptorSets(
				vk::PipelineBindPoint::eCompute,
				gen_pipeline_layout,
				0,
				*resource_set.gen_set,
				{}
			);
			command_buffer.pushConstants<Resolution>(
				gen_pipeline_layout,
				vk::ShaderStageFlagBits::eCompute,
				0,
				resolution
			);

			command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_barrier));
			command_buffer.dispatch(dispatch_size.x, dispatch_size.y, 1);
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

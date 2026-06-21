#include "render/pipeline/shadow/denoise.hpp"
#include "common/number-literals.hpp"
#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "render/interface/camera.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/shadow.hpp"
#include "shader/shadow/spatial-filter.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/base-level.hpp"
#include "vulkan/numeric/pool-size.hpp"
#include "vulkan/util/descriptor-set-layout.hpp"
#include "vulkan/util/shader.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <libassert/assert.hpp>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render::shadow
{
	using Layout = vulkan::MonoDescriptorSetLayout<
		vulkan::MonoDescriptorSetSlot<
			vk::DescriptorType::eCombinedImageSampler,
			vk::ShaderStageFlagBits::eCompute
		>,
		vulkan::MonoDescriptorSetSlot<vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute>,
		vulkan::MonoDescriptorSetSlot<
			vk::DescriptorType::eCombinedImageSampler,
			vk::ShaderStageFlagBits::eCompute
		>,
		vulkan::MonoDescriptorSetSlot<
			vk::DescriptorType::eCombinedImageSampler,
			vk::ShaderStageFlagBits::eCompute
		>,
		vulkan::MonoDescriptorSetSlot<vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eCompute>
	>;

	std::expected<DenoisePipeline, Error> DenoisePipeline::create(const vulkan::Context& context) noexcept
	{
		/*===== Descriptor Set Layout =====*/

		auto input_layout_result = Layout::create_descriptor_set_layout(context);
		if (!input_layout_result)
			return input_layout_result.error().forward("Create descriptor set layout failed");
		auto input_layout = std::move(*input_layout_result);

		/*===== Pipeline Layout =====*/

		const auto push_constant_range = vk::PushConstantRange{
			.stageFlags = vk::ShaderStageFlagBits::eCompute,
			.offset = 0,
			.size = sizeof(PushConstant),
		};
		const auto set_layout = *input_layout;
		auto pipeline_layout_result = context.device.createPipelineLayout(
			vk::PipelineLayoutCreateInfo()
				.setPushConstantRanges(push_constant_range)
				.setSetLayouts(set_layout)
		);
		if (!pipeline_layout_result) return Error::from(pipeline_layout_result);
		auto pipeline_layout = std::move(*pipeline_layout_result);

		/*===== Shader =====*/

		auto shader_module_result = vulkan::create_shader(context.device, shader::shadow::spatial_filter);
		if (!shader_module_result) return shader_module_result.error().forward("Create shader module failed");
		auto shader_module = std::move(*shader_module_result);

		const auto shader_info = vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eCompute,
			.module = shader_module,
			.pName = "main",
		};

		/*===== Pipeline =====*/

		const auto create_info = vk::ComputePipelineCreateInfo{
			.stage = shader_info,
			.layout = pipeline_layout,
		};
		auto pipeline_result = context.device.createComputePipeline(nullptr, create_info);
		if (!pipeline_result) return Error::from(pipeline_result);
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

		return DenoisePipeline(
			std::move(input_layout),
			std::move(pipeline_layout),
			std::move(pipeline),
			std::move(sampler)
		);
	}

	std::expected<std::vector<DenoisePipeline::ResourceSet>, Error> DenoisePipeline::create_resource_sets(
		const vulkan::Context& context,
		uint32_t count
	) const noexcept
	{
		static constexpr auto BINDINGS = Layout::get_bindings();
		const auto pool_sizes = vulkan::calc_pool_sizes(BINDINGS, count * FILTER_PASSES);

		auto pool_result = context.device.createDescriptorPool(
			vk::DescriptorPoolCreateInfo()
				.setPoolSizes(pool_sizes)
				.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
				.setMaxSets(count * FILTER_PASSES)
		);
		if (!pool_result) return Error::from(pool_result);
		auto pool = std::make_shared<vk::raii::DescriptorPool>(std::move(*pool_result));

		const auto layouts = std::vector(count * FILTER_PASSES, *input_layout);
		auto sets_result = context.device.allocateDescriptorSets(
			vk::DescriptorSetAllocateInfo().setSetLayouts(layouts).setDescriptorPool(*pool)
		);
		if (!sets_result) return Error::from(sets_result);
		auto sets = std::move(*sets_result);

		auto set_groups =
			sets
			| std::views::as_rvalue
			| std::views::transform([](auto&& val) {
				  return std::make_unique<vk::raii::DescriptorSet>(std::forward<decltype(val)>(val));
			  })
			| std::views::chunk(FILTER_PASSES)
			| std::views::transform([](auto&& chunk) {
				  std::array<std::unique_ptr<vk::raii::DescriptorSet>, FILTER_PASSES> result;
				  std::ranges::move(chunk, result.begin());
				  return result;
			  })
			| std::ranges::to<std::vector>();

		return std::views::zip_transform(
				   CTOR_LAMBDA(ResourceSet),
				   std::views::repeat(pool, count),
				   std::views::as_rvalue(set_groups),
				   std::views::repeat(*sampler, count)
			   )
			| std::ranges::to<std::vector>();
	}

	void DenoisePipeline::denoise(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource_set
	) const noexcept
	{
		DEBUG_ASSERT(resource_set.resource.has_value());

		command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);

		for (const auto [iter, set] : std::views::enumerate(resource_set.sets))
		{
			const auto output_image = iter % 2 == 0 ? resource_set->denoise_final : resource_set->denoise_imm;

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

			const auto dispatch_size = (resource_set->half_extent + BLOCK_SIZE - 1_u32) / BLOCK_SIZE;

			command_buffer
				.bindDescriptorSets(vk::PipelineBindPoint::eCompute, pipeline_layout, 0, {**set}, {});
			command_buffer.pushConstants<PushConstant>(
				pipeline_layout,
				vk::ShaderStageFlagBits::eCompute,
				0,
				PushConstant{
					.half_size = resource_set->half_extent,
					.stride = 1_u32 << iter,
				}
			);

			command_buffer.pipelineBarrier2(
				vk::DependencyInfo().setImageMemoryBarriers(output_image_pre_barrier)
			);

			command_buffer.dispatch(dispatch_size.x, dispatch_size.y, 1);

			command_buffer.pipelineBarrier2(
				vk::DependencyInfo().setImageMemoryBarriers(output_image_post_barrier)
			);
		}
	}

	void DenoisePipeline::ResourceSet::update(
		const vulkan::Context& context,
		vulkan::ElementBufferRef<Camera> camera,
		HalfDeferredAttachment::View half_gbuffer,
		ShadowAttachment::View shadow
	) noexcept
	{
		DEBUG_ASSERT(half_gbuffer.half_extent == shadow.half_extent);
		DEBUG_ASSERT(half_gbuffer.full_extent == shadow.full_extent);

		const auto init_sample_info_sampled = vk::DescriptorImageInfo{
			.sampler = sampler,
			.imageView = shadow.init_sample.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		const auto denoise_imm_info_sampled = vk::DescriptorImageInfo{
			.sampler = sampler,
			.imageView = shadow.denoise_imm.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		const auto denoise_final_info_sampled = vk::DescriptorImageInfo{
			.sampler = sampler,
			.imageView = shadow.denoise_final.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		const auto denoise_imm_info_storage = vk::DescriptorImageInfo{
			.sampler = nullptr,
			.imageView = shadow.denoise_imm.view,
			.imageLayout = vk::ImageLayout::eGeneral,
		};

		const auto denoise_final_info_storage = vk::DescriptorImageInfo{
			.sampler = nullptr,
			.imageView = shadow.denoise_final.view,
			.imageLayout = vk::ImageLayout::eGeneral,
		};

		const auto normal_tex_info = vk::DescriptorImageInfo{
			.sampler = sampler,
			.imageView = half_gbuffer.geom_normal.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		const auto depth_tex_info = vk::DescriptorImageInfo{
			.sampler = sampler,
			.imageView = half_gbuffer.depth.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		const auto camera_info = vk::DescriptorBufferInfo{
			.buffer = camera,
			.offset = 0,
			.range = vk::WholeSize,
		};

		for (const auto [iter, set] : sets | std::views::as_const | std::views::enumerate)
		{
			const auto input_info = [&] {
				if (iter == 0) return init_sample_info_sampled;
				return iter % 2 == 0 ? denoise_imm_info_sampled : denoise_final_info_sampled;
			}();

			const auto output_info = iter % 2 == 0 ? denoise_final_info_storage : denoise_imm_info_storage;

			const auto write_sets = Layout::get_write_infos(
				*set,
				input_info,
				output_info,
				depth_tex_info,
				normal_tex_info,
				camera_info
			);

			context.device.updateDescriptorSets(write_sets, {});
		}

		resource = Resource{
			.half_extent = shadow.half_extent,
			.init_sample = shadow.init_sample,
			.denoise_imm = shadow.denoise_imm,
			.denoise_final = shadow.denoise_final,
		};
	}
}

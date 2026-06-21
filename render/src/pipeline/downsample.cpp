#include "render/pipeline/downsample.hpp"
#include "common/number-literals.hpp"
#include "common/util/array.hpp"
#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "render/resource/deferred.hpp"
#include "shader/downsample.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/base-level.hpp"
#include "vulkan/numeric/pool-size.hpp"
#include "vulkan/util/descriptor-set-layout.hpp"
#include "vulkan/util/shader.hpp"

#include <array>
#include <cstdint>
#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <libassert/assert.hpp>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	namespace
	{
		using DescriptorSetLayout = vulkan::MonoDescriptorSetLayout<
			vulkan::MonoDescriptorSetSlot<
				vk::DescriptorType::eCombinedImageSampler,
				vk::ShaderStageFlagBits::eCompute
			>,
			vulkan::MonoDescriptorSetSlot<
				vk::DescriptorType::eCombinedImageSampler,
				vk::ShaderStageFlagBits::eCompute
			>,
			vulkan::MonoDescriptorSetSlot<
				vk::DescriptorType::eCombinedImageSampler,
				vk::ShaderStageFlagBits::eCompute
			>,
			vulkan::MonoDescriptorSetSlot<
				vk::DescriptorType::eCombinedImageSampler,
				vk::ShaderStageFlagBits::eCompute
			>,
			vulkan::MonoDescriptorSetSlot<
				vk::DescriptorType::eCombinedImageSampler,
				vk::ShaderStageFlagBits::eCompute
			>,
			vulkan::
				MonoDescriptorSetSlot<vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute>,
			vulkan::
				MonoDescriptorSetSlot<vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute>,
			vulkan::
				MonoDescriptorSetSlot<vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute>,
			vulkan::
				MonoDescriptorSetSlot<vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute>,
			vulkan::
				MonoDescriptorSetSlot<vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eCompute>
		>;

		std::expected<std::pair<vk::raii::DescriptorSetLayout, vk::raii::PipelineLayout>, Error>
		create_layout(const vulkan::Context& context) noexcept
		{
			constexpr auto push_constant_range = vk::PushConstantRange{
				.stageFlags = vk::ShaderStageFlagBits::eCompute,
				.offset = 0,
				.size = sizeof(glm::u32vec2),
			};

			auto descriptor_set_layout_result = DescriptorSetLayout::create_descriptor_set_layout(context);
			if (!descriptor_set_layout_result)
				return descriptor_set_layout_result.error().forward("Create descirptor set layout failed");
			auto descriptor_set_layout = std::move(*descriptor_set_layout_result);
			const auto descriptor_set_layout_raw = *descriptor_set_layout;

			auto pipeline_layout_result = context.device.createPipelineLayout(
				vk::PipelineLayoutCreateInfo()
					.setSetLayouts(descriptor_set_layout_raw)
					.setPushConstantRanges(push_constant_range)
			);
			if (!pipeline_layout_result) return Error::from(pipeline_layout_result);
			auto pipeline_layout = std::move(*pipeline_layout_result);

			return std::make_pair(std::move(descriptor_set_layout), std::move(pipeline_layout));
		}

		std::expected<vk::raii::Pipeline, Error> create_pipeline(
			const vulkan::Context& context,
			vk::PipelineLayout pipeline_layout,
			vk::ShaderModule shader_module
		) noexcept
		{
			const auto shader_stage = vk::PipelineShaderStageCreateInfo{
				.stage = vk::ShaderStageFlagBits::eCompute,
				.module = shader_module,
				.pName = "main",
				.pSpecializationInfo = nullptr
			};

			const auto create_info = vk::ComputePipelineCreateInfo{
				.stage = shader_stage,
				.layout = pipeline_layout,
			};
			auto pipeline_result = context.device.createComputePipeline(nullptr, create_info);
			if (!pipeline_result) return Error::from(pipeline_result);

			return std::move(*pipeline_result);
		}

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
		auto shader_module_result = vulkan::create_shader(context.device, shader::downsample);
		if (!shader_module_result) return shader_module_result.error().forward("Create shader module failed");
		auto shader_module = std::move(*shader_module_result);

		auto layout_result = create_layout(context);
		if (!layout_result) return layout_result.error().forward("Create layout failed");
		auto [descriptor_set_layout, pipeline_layout] = std::move(*layout_result);

		auto pipeline_result = create_pipeline(context, pipeline_layout, shader_module);
		if (!pipeline_result) return pipeline_result.error().forward("Create downsample pipeline failed");
		auto pipeline = std::move(*pipeline_result);

		auto sampler_result = create_sampler(context);
		if (!sampler_result) return sampler_result.error().forward("Create downsample sampler failed");
		auto sampler = std::move(*sampler_result);

		return DownsamplePipeline(
			std::move(descriptor_set_layout),
			std::move(pipeline_layout),
			std::move(pipeline),
			std::move(sampler)
		);
	}

	std::expected<std::vector<DownsamplePipeline::ResourceSet>, Error>
	DownsamplePipeline::create_resource_sets(const vulkan::Context& context, uint32_t count) const noexcept
	{
		const auto pool_sizes = vulkan::calc_pool_sizes(DescriptorSetLayout::get_bindings(), count);
		auto pool_result = context.device.createDescriptorPool(
			vk::DescriptorPoolCreateInfo()
				.setPoolSizes(pool_sizes)
				.setMaxSets(count)
				.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
		);
		if (!pool_result) return Error::from(pool_result);
		auto pool = std::make_shared<vk::raii::DescriptorPool>(std::move(*pool_result));

		const auto data_layouts = std::vector(count, *descriptor_set_layout);
		auto sets_result = context.device.allocateDescriptorSets(
			vk::DescriptorSetAllocateInfo()
				.setSetLayouts(data_layouts)
				.setDescriptorSetCount(count)
				.setDescriptorPool(*pool)
		);
		if (!sets_result) return Error::from(sets_result);
		auto sets = std::move(*sets_result);

		return std::vector(
			std::from_range,
			std::views::zip_transform(
				CTOR_LAMBDA(ResourceSet),
				std::views::repeat(pool, count),
				sets | std::views::as_rvalue,
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

		command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
		command_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eCompute,
			pipeline_layout,
			0,
			{resource_set.descriptor_set},
			{}
		);
		command_buffer.pushConstants<PushConstant>(
			pipeline_layout,
			vk::ShaderStageFlagBits::eCompute,
			0,
			{resource_set.attachment->half_extent}
		);

		const auto dispatch_size = (resource_set.attachment->half_extent + BLOCK_SIZE - 1_u32) / BLOCK_SIZE;
		command_buffer.dispatch(dispatch_size.x, dispatch_size.y, 1);

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
		const auto full_tex = std::to_array<vk::ImageView>({
			deferred.albedo.view,
			deferred.normal.view,
			deferred.geom_normal.view,
			deferred.pbr.view,
			deferred.depth.view,
		});

		const auto half_tex = std::to_array<vk::ImageView>({
			halfres_deferred.albedo.view,
			halfres_deferred.normal.view,
			halfres_deferred.geom_normal.view,
			halfres_deferred.pbr.view,
			halfres_deferred.depth.view,
		});

		const auto full_info =
			full_tex | util::map_array([this](vk::ImageView view) {
				return vk::DescriptorImageInfo{
					.sampler = texture_sampler,
					.imageView = view,
					.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
				};
			});

		const auto half_info =
			half_tex | util::map_array([](vk::ImageView view) {
				return vk::DescriptorImageInfo{
					.sampler = nullptr,
					.imageView = view,
					.imageLayout = vk::ImageLayout::eGeneral
				};
			});

		const auto infos = util::array_concat(full_info, half_info) | util::array_to_tuple;
		const auto write_sets = DescriptorSetLayout::get_write_infos(descriptor_set, infos);
		context.device.updateDescriptorSets(write_sets, {});

		attachment = halfres_deferred;
	}
}

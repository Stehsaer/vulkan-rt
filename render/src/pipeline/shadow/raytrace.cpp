#include "render/pipeline/shadow/raytrace.hpp"
#include "common/json.hpp"
#include "common/number-literals.hpp"
#include "common/util/align.hpp"
#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "common/util/span.hpp"
#include "render/interface/camera.hpp"
#include "render/interface/direct-light.hpp"
#include "render/model/material.hpp"
#include "render/model/tlas.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/raytrace.hpp"
#include "render/resource/shadow.hpp"
#include "shader/shadow/trace.hpp"
#include "vulkan/alloc/allocator.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/base-level.hpp"
#include "vulkan/numeric/pool-size.hpp"
#include "vulkan/util/command-runner.hpp"
#include "vulkan/util/descriptor-set-layout.hpp"
#include "vulkan/util/shader.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <glm/ext/vector_int2_sized.hpp>
#include <libassert/assert.hpp>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render::shadow
{
	using vulkan::MonoDescriptorSetLayout;
	using vulkan::MonoDescriptorSetSlot;

	using InputLayout = MonoDescriptorSetLayout<
		MonoDescriptorSetSlot<
			vk::DescriptorType::eAccelerationStructureKHR,
			vk::ShaderStageFlagBits::eRaygenKHR
		>,
		MonoDescriptorSetSlot<vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR>,
		MonoDescriptorSetSlot<vk::DescriptorType::eUniformBuffer, vk::ShaderStageFlagBits::eRaygenKHR>,
		MonoDescriptorSetSlot<vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR>,
		MonoDescriptorSetSlot<vk::DescriptorType::eStorageImage, vk::ShaderStageFlagBits::eRaygenKHR>,
		MonoDescriptorSetSlot<vk::DescriptorType::eCombinedImageSampler, vk::ShaderStageFlagBits::eRaygenKHR>
	>;

	std::expected<vk::raii::PipelineLayout, Error> RaytracePipeline::create_pipeline_layout(
		const vulkan::Context& context,
		vk::DescriptorSetLayout material_layout,
		vk::DescriptorSetLayout mesh_resource_layout,
		vk::DescriptorSetLayout input_layout
	) noexcept
	{
		const auto layout_list = std::to_array({
			material_layout,
			mesh_resource_layout,
			input_layout,
		});

		const auto push_constant_range = vk::PushConstantRange{
			.stageFlags = vk::ShaderStageFlagBits::eRaygenKHR,
			.offset = 0,
			.size = sizeof(PushConstant),
		};

		const auto create_info =
			vk::PipelineLayoutCreateInfo()
				.setSetLayouts(layout_list)
				.setPushConstantRanges(push_constant_range);

		auto layout_result = context.device.createPipelineLayout(create_info);
		if (!layout_result) return Error::from(layout_result);

		return std::move(*layout_result);
	}

	namespace
	{
		constexpr auto SHADER_GROUP_HANDLE_SIZE = 32_u32;
		constexpr auto MAX_RECURSION_DEPTH = 4_u32;

		using ShaderGroupHandle = std::array<std::byte, SHADER_GROUP_HANDLE_SIZE>;

		enum class StageIndex : uint32_t
		{
			Raygen = 0,
			Miss,
			Anyhit
		};

		enum class GroupIndex : uint32_t
		{
			Raygen = 0,
			Miss,
			Hit,
			GroupCount
		};

		std::expected<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR, Error> check_feature(
			const vulkan::Context& context
		) noexcept
		{
			if (!context.feature.raytracing) return Error("Missing required 'raytracing' feature");

			const auto properties =
				context.phy_device
					.getProperties2<
						vk::PhysicalDeviceProperties2,
						vk::PhysicalDeviceRayTracingPipelinePropertiesKHR
					>()
					.get<vk::PhysicalDeviceRayTracingPipelinePropertiesKHR>();

			/* HANDLE SIZE CHECK
			 * Vulkan specification mandates handle size to be 32. Reject all non-compliant devices.
			 * See Section 56.1 "Limit Requirements" in vulkan spec 1.4.352
			 */

			if (properties.shaderGroupHandleSize != 32)
				return Error(
					"Non standard-compliant device found",
					std::format(
						"Vulkan spec mandates shaderGroupHandleSize to be 32, but got {}",
						properties.shaderGroupHandleSize
					)
				);

			if (properties.maxRayRecursionDepth < MAX_RECURSION_DEPTH)
			{
				Json diag = {
					{"device", {"max_recursion_depth", properties.maxRayRecursionDepth}},
				};

				return Error(
					"Physical device doesn't meet minimum requirements for raytracing pipeline",
					std::format("Required minimum max recursion depth not met"),
					std::move(diag)
				);
			}

			return properties;
		}

		std::expected<vk::raii::Pipeline, Error> create_pipeline(
			const vulkan::Context& context,
			vk::PipelineLayout layout
		) noexcept
		{
			/*===== Shaders =====*/

			static constexpr auto RAYGEN_NAME = "raygen_main";
			static constexpr auto ANYHIT_NAME = "anyhit_main";
			static constexpr auto MISS_NAME = "miss_main";

			auto shader_result = vulkan::create_shader(context.device, shader::shadow::trace);
			if (!shader_result) return shader_result.error().forward("Create shader module failed");
			auto shader = std::move(*shader_result);

			const auto raygen_stage = vk::PipelineShaderStageCreateInfo{
				.stage = vk::ShaderStageFlagBits::eRaygenKHR,
				.module = shader,
				.pName = RAYGEN_NAME,
			};

			const auto anyhit_stage = vk::PipelineShaderStageCreateInfo{
				.stage = vk::ShaderStageFlagBits::eAnyHitKHR,
				.module = shader,
				.pName = ANYHIT_NAME,
			};

			const auto miss_stage = vk::PipelineShaderStageCreateInfo{
				.stage = vk::ShaderStageFlagBits::eMissKHR,
				.module = shader,
				.pName = MISS_NAME,
			};

			const auto stages = std::to_array({
				raygen_stage,
				miss_stage,
				anyhit_stage,
			});

			/*===== Shader Groups =====*/

			static constexpr auto raygen_group = vk::RayTracingShaderGroupCreateInfoKHR{
				.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
				.generalShader = std::to_underlying(StageIndex::Raygen),
			};

			static constexpr auto miss_group = vk::RayTracingShaderGroupCreateInfoKHR{
				.type = vk::RayTracingShaderGroupTypeKHR::eGeneral,
				.generalShader = std::to_underlying(StageIndex::Miss),
			};

			static constexpr auto hit_group = vk::RayTracingShaderGroupCreateInfoKHR{
				.type = vk::RayTracingShaderGroupTypeKHR::eTrianglesHitGroup,
				.closestHitShader = vk::ShaderUnusedKHR,
				.anyHitShader = std::to_underlying(StageIndex::Anyhit),
			};

			static constexpr auto shader_groups = std::to_array({
				raygen_group,
				miss_group,
				hit_group,
			});

			/*===== Create Pipeline =====*/

			static constexpr auto MAX_RECURSION_DEPTH = 4_u32;

			const auto pipeline_create_info =
				vk::RayTracingPipelineCreateInfoKHR()
					.setMaxPipelineRayRecursionDepth(MAX_RECURSION_DEPTH)
					.setGroups(shader_groups)
					.setLayout(layout)
					.setStages(stages);
			auto pipeline_result =
				context.device.createRayTracingPipelineKHR(nullptr, nullptr, pipeline_create_info);
			if (!pipeline_result) return Error::from(pipeline_result);
			auto pipeline = std::move(*pipeline_result);

			return pipeline;
		}
	}

	std::expected<RaytracePipeline, Error> RaytracePipeline::create(
		const vulkan::Context& context,
		const MaterialLayout& material_layout,
		const RaytraceResourceLayout& raytrace_layout
	) noexcept
	{
		const auto check_result = check_feature(context);
		if (!check_result) return check_result.error();
		const auto properties = *check_result;

		/*===== Create Layouts =====*/

		auto input_layout_result = InputLayout::create_descriptor_set_layout(context);
		if (!input_layout_result)
			return input_layout_result.error().forward("Create input descriptor set layout failed");
		auto input_layout = std::move(*input_layout_result);

		auto pipeline_layout_result = create_pipeline_layout(
			context,
			material_layout.layout,
			raytrace_layout->mesh_resource,
			input_layout
		);
		if (!pipeline_layout_result)
			return pipeline_layout_result.error().forward("Create pipeline layout failed");
		auto pipeline_layout = std::move(*pipeline_layout_result);

		/*===== Create Pipeline =====*/

		auto pipeline_result = create_pipeline(context, pipeline_layout);
		if (!pipeline_result) return pipeline_result.error();
		auto pipeline = std::move(*pipeline_result);

		/*===== Get Shader Group Handles =====*/

		auto shader_group_handles_result =
			(*context.device)
				.getRayTracingShaderGroupHandlesKHR<ShaderGroupHandle>(
					pipeline,
					0,
					std::to_underlying(GroupIndex::GroupCount),
					SHADER_GROUP_HANDLE_SIZE * std::to_underlying(GroupIndex::GroupCount),
					*context.device.getDispatcher()
				);
		if (!shader_group_handles_result) return Error::from(shader_group_handles_result);
		auto shader_group_handles = std::move(*shader_group_handles_result);

		/*===== Calculate and Create SBT Buffer =====*/

		// NOTE: Use conservative alignment that satisfy both requirement
		const uint32_t alignment = std::max({
			SHADER_GROUP_HANDLE_SIZE,
			properties.shaderGroupHandleAlignment,
			properties.shaderGroupBaseAlignment,
		});

		auto sbt_buffer_result = context.allocator.create_buffer(
			{
				// NOTE: additional `conservative_alignment` ensures enough room for alignment
				.size = alignment * (std::to_underlying(GroupIndex::GroupCount) + 1),
				.usage = vk::BufferUsageFlagBits::eShaderBindingTableKHR
					| vk::BufferUsageFlagBits::eShaderDeviceAddress
					| vk::BufferUsageFlagBits::eTransferDst,
			},
			vulkan::MemoryUsage::GpuOnly
		);
		auto sbt_staging_buffer_result = context.allocator.create_buffer(
			{
				.size = alignment * std::to_underlying(GroupIndex::GroupCount),
				.usage = vk::BufferUsageFlagBits::eTransferSrc,
			},
			vulkan::MemoryUsage::CpuToGpu
		);

		if (!sbt_buffer_result) return sbt_buffer_result.error().forward("Create SBT buffer failed");
		if (!sbt_staging_buffer_result)
			return sbt_staging_buffer_result.error().forward("Create SBT staging buffer failed");

		auto sbt_buffer = std::move(*sbt_buffer_result);
		auto sbt_staging_buffer = std::move(*sbt_staging_buffer_result);

		/*===== Build SBT Data =====*/

		auto sbt_data =
			std::vector<std::byte>(alignment * std::to_underlying(GroupIndex::GroupCount), std::byte(0));
		for (const auto [idx, handle] : std::views::enumerate(shader_group_handles))
			std::ranges::copy(handle, sbt_data.begin() + idx * alignment);
		if (const auto result = sbt_staging_buffer.upload(util::as_bytes(sbt_data), 0); !result)
			return result.error().forward("Save SBT data to staging buffer failed");

		/*===== Upload SBT Data =====*/

		const auto sbt_base_address = context.device.getBufferAddress({.buffer = sbt_buffer});
		const auto sbt_aligned_base_address = util::align_address(sbt_base_address, alignment);
		const auto sbt_start_offset = sbt_aligned_base_address - sbt_base_address;

		auto runner_result = vulkan::CommandRunner::create(context);
		if (!runner_result) return runner_result.error().forward("Create command runner failed");
		auto runner = std::move(*runner_result);

		const auto run_result = runner.run(
			context,
			[&sbt_staging_buffer, &sbt_buffer, sbt_start_offset, alignment](
				const vk::raii::CommandBuffer& command_buffer
			) {
				const auto region = vk::BufferCopy2{
					.srcOffset = 0,
					.dstOffset = sbt_start_offset,
					.size = alignment * std::to_underlying(GroupIndex::GroupCount)
				};

				command_buffer.copyBuffer2(
					vk::CopyBufferInfo2()
						.setSrcBuffer(sbt_staging_buffer)
						.setDstBuffer(sbt_buffer)
						.setRegions(region)
				);

				const auto post_transfer_barrier = vk::BufferMemoryBarrier2{
					.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
					.srcAccessMask = vk::AccessFlagBits2::eMemoryWrite,
					.dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
					.dstAccessMask = vk::AccessFlagBits2::eMemoryRead,
					.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
					.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
					.buffer = sbt_buffer,
					.offset = 0,
					.size = vk::WholeSize,
				};

				command_buffer.pipelineBarrier2(
					vk::DependencyInfo().setBufferMemoryBarriers(post_transfer_barrier)
				);
			}
		);
		if (!run_result) return run_result.error().forward("Upload SBT data failed");

		const auto raygen_region = vk::StridedDeviceAddressRegionKHR{
			.deviceAddress = sbt_aligned_base_address + std::to_underlying(GroupIndex::Raygen) * alignment,
			.stride = alignment,
			.size = alignment
		};

		const auto miss_region = vk::StridedDeviceAddressRegionKHR{
			.deviceAddress = sbt_aligned_base_address + std::to_underlying(GroupIndex::Miss) * alignment,
			.stride = alignment,
			.size = alignment
		};

		const auto hit_region = vk::StridedDeviceAddressRegionKHR{
			.deviceAddress = sbt_aligned_base_address + std::to_underlying(GroupIndex::Hit) * alignment,
			.stride = alignment,
			.size = alignment
		};

		/*===== Sampler =====*/

		const auto depth_sampler_info = vk::SamplerCreateInfo{
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
			.unnormalizedCoordinates = vk::False
		};

		auto depth_sampler_result = context.device.createSampler(depth_sampler_info);
		if (!depth_sampler_result) return Error::from(depth_sampler_result);
		auto depth_sampler = std::move(*depth_sampler_result);

		const auto noise_sampler_info = vk::SamplerCreateInfo{
			.magFilter = vk::Filter::eNearest,
			.minFilter = vk::Filter::eNearest,
			.mipmapMode = vk::SamplerMipmapMode::eNearest,
			.addressModeU = vk::SamplerAddressMode::eRepeat,
			.addressModeV = vk::SamplerAddressMode::eRepeat,
			.addressModeW = vk::SamplerAddressMode::eRepeat,
			.mipLodBias = 0,
			.anisotropyEnable = vk::False,
			.maxAnisotropy = 0,
			.compareEnable = vk::False,
			.minLod = 0,
			.maxLod = 0,
			.unnormalizedCoordinates = vk::False
		};

		auto noise_sampler_result = context.device.createSampler(noise_sampler_info);
		if (!noise_sampler_result) return Error::from(noise_sampler_result);
		auto noise_sampler = std::move(*noise_sampler_result);

		/*===== Finish =====*/

		return RaytracePipeline(
			std::move(input_layout),
			std::move(pipeline_layout),
			std::move(pipeline),
			std::move(sbt_buffer),
			raygen_region,
			miss_region,
			hit_region,
			std::move(depth_sampler),
			std::move(noise_sampler)
		);
	}

	std::expected<std::vector<RaytracePipeline::ResourceSet>, Error> RaytracePipeline::create_resource_sets(
		const vulkan::Context& context,
		uint32_t count
	) const noexcept
	{
		static constexpr auto BINDINGS = InputLayout::get_bindings();
		const auto pool_sizes = vulkan::calc_pool_sizes(BINDINGS, count);

		auto pool_result = context.device.createDescriptorPool(
			vk::DescriptorPoolCreateInfo()
				.setPoolSizes(pool_sizes)
				.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
				.setMaxSets(count)
		);
		if (!pool_result) return Error::from(pool_result);
		auto pool = std::make_shared<vk::raii::DescriptorPool>(std::move(*pool_result));

		const auto layouts = std::vector(count, *input_layout);
		auto sets_result = context.device.allocateDescriptorSets(
			vk::DescriptorSetAllocateInfo().setSetLayouts(layouts).setDescriptorPool(*pool)
		);
		if (!sets_result) return Error::from(sets_result);
		auto sets = std::move(*sets_result);

		return std::views::zip_transform(
				   CTOR_LAMBDA(ResourceSet),
				   std::views::repeat(pool, count),
				   std::views::as_rvalue(sets),
				   std::views::repeat(*depth_sampler, count),
				   std::views::repeat(*noise_sampler, count)
			   )
			| std::ranges::to<std::vector>();
	}

	void RaytracePipeline::trace(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource
	) const noexcept
	{
		DEBUG_ASSERT(resource.resource.has_value());

		const auto pre_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = {},
			.srcAccessMask = {},
			.dstStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
			.dstAccessMask = vk::AccessFlagBits2::eShaderWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eGeneral,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = resource->shadow_tex.image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		};

		const auto post_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eRayTracingShaderKHR,
			.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eAllGraphics,
			.dstAccessMask = vk::AccessFlagBits2::eShaderRead,
			.oldLayout = vk::ImageLayout::eGeneral,
			.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = resource->shadow_tex.image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		};

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(pre_barrier));

		command_buffer.bindPipeline(vk::PipelineBindPoint::eRayTracingKHR, pipeline);
		command_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eRayTracingKHR,
			pipeline_layout,
			0,
			{resource->material_set, resource->raytrace_res_set, resource.input_set},
			{}
		);
		command_buffer.pushConstants<PushConstant>(
			pipeline_layout,
			vk::ShaderStageFlagBits::eRaygenKHR,
			0,
			PushConstant{
				.full_size = resource->full_extent,
				.noise_offset = resource->noise_offset,
			}
		);

		command_buffer.traceRaysKHR(
			raygen_region,
			miss_region,
			hitgroup_region,
			{
				.deviceAddress = hitgroup_region.deviceAddress + hitgroup_region.size,
				.stride = 0,
				.size = 0,
			},
			resource->half_extent.x,
			resource->half_extent.y,
			1
		);

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(post_barrier));
	}

	void RaytracePipeline::ResourceSet::update(
		const vulkan::Context& context,
		const MaterialList& material,
		const Tlas& tlas,
		const RaytraceResource& raytrace_res,
		HalfDeferredAttachment::View gbuffer,
		ShadowAttachment::View attachment,
		vulkan::ElementBufferRef<Camera> camera,
		vulkan::ElementBufferRef<DirectLight> direct_light,
		vk::ImageView noise_tex,
		glm::i32vec2 noise_offset
	) noexcept
	{
		DEBUG_ASSERT(gbuffer->half_extent == attachment->half_extent);
		DEBUG_ASSERT(gbuffer->full_extent == attachment->full_extent);

		const vk::AccelerationStructureKHR tlas_instance = tlas;
		const auto tlas_write_info =
			vk::WriteDescriptorSetAccelerationStructureKHR().setAccelerationStructures(tlas_instance);

		const auto direct_light_buffer_info = vk::DescriptorBufferInfo{
			.buffer = direct_light,
			.offset = 0,
			.range = vk::WholeSize,
		};

		const auto camera_buffer_info = vk::DescriptorBufferInfo{
			.buffer = camera,
			.offset = 0,
			.range = vk::WholeSize,
		};

		const auto depth_tex_info = vk::DescriptorImageInfo{
			.sampler = depth_sampler,
			.imageView = gbuffer.depth.view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
		};

		const auto shadow_tex_info = vk::DescriptorImageInfo{
			.sampler = nullptr,
			.imageView = attachment.shadow.view,
			.imageLayout = vk::ImageLayout::eGeneral
		};

		const auto noise_tex_info = vk::DescriptorImageInfo{
			.sampler = noise_sampler,
			.imageView = noise_tex,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};

		const auto write_infos = InputLayout::get_write_infos(
			input_set,
			tlas_write_info,
			direct_light_buffer_info,
			camera_buffer_info,
			depth_tex_info,
			shadow_tex_info,
			noise_tex_info
		);

		context.device.updateDescriptorSets(write_infos, {});

		resource = Resource{
			.full_extent = attachment.full_extent,
			.half_extent = attachment.half_extent,
			.noise_offset = noise_offset,
			.shadow_tex = attachment.shadow,
			.material_set = material.get_descriptor_set(),
			.raytrace_res_set = raytrace_res->mesh_resource
		};
	}
}

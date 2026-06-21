#pragma once

#include "common/util/error.hpp"
#include "render/interface/camera.hpp"
#include "render/interface/direct-light.hpp"
#include "render/model/material.hpp"
#include "render/model/tlas.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/motion-vector.hpp"
#include "render/resource/raytrace.hpp"
#include "render/resource/shadow.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/alloc/buffer.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"

#include <cstdint>
#include <expected>
#include <glm/ext/vector_int2_sized.hpp>
#include <glm/ext/vector_uint2_sized.hpp>
#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render::shadow
{
	///
	/// @brief Shadow raytracing pipeline
	/// @details
	/// - Use raytracing to get shadow visibility at half resolution
	/// - Perform temporal accumulation
	///
	class RaytracePipeline
	{
	  public:

		class ResourceSet;

		///
		/// @brief Create a shadow raytracing pipeline
		///
		/// @param context Vulkan context
		/// @param material_layout Material layout
		/// @param raytrace_layout Raytracing resource layout
		/// @return Created pipeline or error
		///
		[[nodiscard]]
		static std::expected<RaytracePipeline, Error> create(
			const vulkan::Context& context,
			const MaterialLayout& material_layout,
			const RaytraceResourceLayout& raytrace_layout
		) noexcept;

		///
		/// @brief Create resource sets
		///
		/// @param context Vulkan context
		/// @param count Number of resource sets
		/// @return Created resource sets or error
		///
		[[nodiscard]]
		std::expected<std::vector<ResourceSet>, Error> create_resource_sets(
			const vulkan::Context& context,
			uint32_t count
		) const noexcept;

		///
		/// @brief Execute raytracing
		///
		/// @param command_buffer Command buffer
		/// @param resource Resource set of current frame
		///
		void trace(const vk::raii::CommandBuffer& command_buffer, const ResourceSet& resource) const noexcept;

	  private:

		struct PushConstant
		{
			glm::u32vec2 full_size;
			glm::i32vec2 noise_offset;
		};

		vk::raii::DescriptorSetLayout input_layout;
		vk::raii::PipelineLayout pipeline_layout;

		vk::raii::Pipeline pipeline;

		vulkan::Buffer sbt_buffer;

		vk::StridedDeviceAddressRegionKHR raygen_region;
		vk::StridedDeviceAddressRegionKHR miss_region;
		vk::StridedDeviceAddressRegionKHR hitgroup_region;

		vk::raii::Sampler sampler;

		static std::expected<vk::raii::PipelineLayout, Error> create_pipeline_layout(
			const vulkan::Context& context,
			vk::DescriptorSetLayout material_layout,
			vk::DescriptorSetLayout mesh_resource_layout,
			vk::DescriptorSetLayout input_layout
		) noexcept;

		explicit RaytracePipeline(
			vk::raii::DescriptorSetLayout input_layout,
			vk::raii::PipelineLayout pipeline_layout,
			vk::raii::Pipeline pipeline,
			vulkan::Buffer sbt_buffer,
			vk::StridedDeviceAddressRegionKHR raygen_region,
			vk::StridedDeviceAddressRegionKHR miss_region,
			vk::StridedDeviceAddressRegionKHR hitgroup_region,
			vk::raii::Sampler sampler
		) :
			input_layout(std::move(input_layout)),
			pipeline_layout(std::move(pipeline_layout)),
			pipeline(std::move(pipeline)),
			sbt_buffer(std::move(sbt_buffer)),
			raygen_region(raygen_region),
			miss_region(miss_region),
			hitgroup_region(hitgroup_region),
			sampler(std::move(sampler))
		{}

	  public:

		RaytracePipeline(const RaytracePipeline&) = delete;
		RaytracePipeline(RaytracePipeline&&) = default;
		RaytracePipeline& operator=(const RaytracePipeline&) = delete;
		RaytracePipeline& operator=(RaytracePipeline&&) = default;
	};

	///
	/// @brief Resource set for shadow raytracing pipeline
	///
	class RaytracePipeline::ResourceSet
	{
	  public:

		///
		/// @brief Update resource set
		///
		/// @param context Vulkan context
		/// @param material Material list from model
		/// @param tlas TLAS instance
		/// @param raytrace_res Raytracing resource
		/// @param gbuffer Half-resolution deferred attachment (gbuffer)
		/// @param attachment Shadow attachment
		/// @param camera Camera parameter buffer
		/// @param direct_light Direct-light parameter buffer
		/// @param noise_tex Noise texture image view
		/// @param noise_offset Offset for noise texture, which is supposed to be random across frames
		///
		void update(
			const vulkan::Context& context,
			const MaterialList& material,
			const Tlas& tlas,
			const RaytraceResource& raytrace_res,
			HalfDeferredAttachment::View gbuffer,
			MotionVectorAttachment::View motion_vector,
			ShadowAttachment::View attachment,
			ShadowAttachment::View prev_attachment,
			vulkan::ElementBufferRef<Camera> camera,
			vulkan::ElementBufferRef<DirectLight> direct_light,
			vk::ImageView noise_tex,
			glm::i32vec2 noise_offset
		) noexcept;

	  private:

		std::shared_ptr<vk::raii::DescriptorPool> pool;
		vk::raii::DescriptorSet input_set;
		vk::Sampler sampler;

		struct Resource
		{
			glm::u32vec2 full_extent;
			glm::u32vec2 half_extent;
			glm::i32vec2 noise_offset;

			vulkan::AttachmentView shadow_tex;

			vk::DescriptorSet material_set;
			vk::DescriptorSet raytrace_res_set;
		};

		std::optional<Resource> resource = std::nullopt;
		auto operator->() const noexcept { return resource.operator->(); }

		friend RaytracePipeline;

		ResourceSet(
			std::shared_ptr<vk::raii::DescriptorPool> pool,
			vk::raii::DescriptorSet input_set,
			vk::Sampler depth_sampler
		) :
			pool(std::move(pool)),
			input_set(std::move(input_set)),
			sampler(depth_sampler)
		{}

	  public:

		ResourceSet(const ResourceSet&) = delete;
		ResourceSet(ResourceSet&&) = default;
		ResourceSet& operator=(const ResourceSet&) = delete;
		ResourceSet& operator=(ResourceSet&&) = default;
	};
}

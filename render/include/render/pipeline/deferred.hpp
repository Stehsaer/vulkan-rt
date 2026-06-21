#pragma once

#include <cstdint>
#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "common/util/error.hpp"
#include "model/mesh.hpp"
#include "render/interface/camera.hpp"
#include "render/interface/indirect-drawcall.hpp"
#include "render/model/material.hpp"
#include "render/model/model.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/hdr.hpp"
#include "render/resource/host.hpp"
#include "render/resource/indirect.hpp"
#include "render/util/per-render-state.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"

namespace render
{
	///
	/// @brief Deferred rendering pipeline
	/// @details
	/// - Takes the indirect drawcalls and render to deferred attachment
	/// - Supports 4 material variants, where BLEND is currently rendered as MASK
	///
	/// ### Color attachments
	///
	/// | Location | Description  | R         | G         | B        | A        |
	/// | :------: | :----------: | :-------: | :-------: | :------: | :------: |
	/// | 0        | Albedo       | Albedo R  | Albedo G  | Albedo B | Sky Flag |
	/// | 1        | Normal       | Oct. X    | Oct. Y    | -        | -        |
	/// | 2        | Geom. Normal | Oct. X    | Oct. Y    | -        | -        |
	/// | 3        | PBR          | Roughness | Metalness | -        | -        |
	/// | 4        | HDR Output   | HDR R     | HDR G     | HDR B    | Alpha    |
	///
	/// @note Synchronization scheme used by this pipeline expects next usage of the HDR attachment is color
	/// attachment (which is very likely to be lighting pass)
	///
	class DeferredPipeline
	{
	  public:

		class ResourceSet;

		///
		/// @brief Create a deferred rendering pipeline
		///
		/// @param context Vulkan context
		/// @param material_layout Model material layout
		/// @return Created deferred rendering pipeline, or error if creation failed
		///
		[[nodiscard]]
		static std::expected<DeferredPipeline, Error> create(
			const vulkan::Context& context,
			const render::MaterialLayout& material_layout
		) noexcept;

		///
		/// @brief Create a given number of resource sets
		///
		/// @param context Vulkan context
		/// @param count Number of resource sets to create
		/// @return Created resource sets or error
		///
		[[nodiscard]]
		std::expected<std::vector<ResourceSet>, Error> create_resource_sets(
			const vulkan::Context& context,
			uint32_t count
		) const noexcept;

		///
		/// @brief Render the scene using the deferred rendering pipeline
		///
		/// @param command_buffer Command buffer
		/// @param resource_set Resource set
		///
		void render(
			const vk::raii::CommandBuffer& command_buffer,
			const ResourceSet& resource_set
		) const noexcept;

	  private:

		struct SpecializationConstant
		{
			vk::Bool32 alpha_mask_enabled;
			vk::Bool32 double_sided;
		};

		static std::expected<vk::raii::Pipeline, Error> create_pipeline(
			const vulkan::Context& context,
			vk::PipelineLayout pipeline_layout,
			vk::ShaderModule shader_module,
			bool alpha_mask_enabled,
			bool double_sided
		) noexcept;

		vk::raii::DescriptorSetLayout data_descriptor_set_layout;
		vk::raii::PipelineLayout pipeline_layout;
		PerRenderState<vk::raii::Pipeline> pipelines;

		explicit DeferredPipeline(
			vk::raii::DescriptorSetLayout data_descriptor_set_layout,
			vk::raii::PipelineLayout pipeline_layout,
			PerRenderState<vk::raii::Pipeline> pipelines
		) :
			data_descriptor_set_layout(std::move(data_descriptor_set_layout)),
			pipeline_layout(std::move(pipeline_layout)),
			pipelines(std::move(pipelines))
		{}

	  public:

		DeferredPipeline(const DeferredPipeline&) = delete;
		DeferredPipeline(DeferredPipeline&&) = default;
		DeferredPipeline& operator=(const DeferredPipeline&) = delete;
		DeferredPipeline& operator=(DeferredPipeline&&) = default;
	};

	///
	/// @brief Resource set for deferred pipeline
	///
	class DeferredPipeline::ResourceSet
	{
	  public:

		///
		/// @brief Update the resource set
		///
		/// @param context Vulkan context
		/// @param model Model instance
		/// @param host_drawcall Host drawcall resource
		/// @param indirect_resource Indirect drawcall resource
		/// @param deferred Deferred attachments
		/// @param hdr HDR attachments
		/// @param camera_param Camera parameter buffer
		///
		/// @warning Deferred and HDR attachments must have identical extents, or a fatal/unrecoverable error
		/// will occur
		///
		void update(
			const vulkan::Context& context,
			const Model& model,
			const HostDrawcallResource& host_drawcall,
			const IndirectResource& indirect_resource,
			DeferredAttachment::View deferred,
			HdrAttachment::View hdr,
			vulkan::ElementBufferRef<Camera> camera_param
		) noexcept;

	  private:

		std::shared_ptr<vk::raii::DescriptorPool> descriptor_pool;
		PerRenderState<vk::raii::DescriptorSet> data_descriptor_set;

		struct Attachment
		{
			glm::u32vec2 extent;
			vulkan::AttachmentView albedo, normal, geom_normal, pbr, depth, hdr;
		};

		// External resources
		struct Resource
		{
			vk::DescriptorSet material_descriptor_set;

			vulkan::ArrayBufferRef<model::FullVertex> vertex_buffer;
			vulkan::ArrayBufferRef<uint32_t> index_buffer;
			PerRenderState<vulkan::ArrayBufferRef<IndirectDrawcall>> indirect_buffers;

			Attachment attachment;
		};

		std::optional<Resource> resource = std::nullopt;

		const Resource* operator->() const noexcept { return resource.operator->(); }

		explicit ResourceSet(
			std::shared_ptr<vk::raii::DescriptorPool> descriptor_pool,
			PerRenderState<vk::raii::DescriptorSet> data_descriptor_set
		) :
			descriptor_pool(std::move(descriptor_pool)),
			data_descriptor_set(std::move(data_descriptor_set))
		{}

		friend class DeferredPipeline;

	  public:

		ResourceSet(const ResourceSet&) = delete;
		ResourceSet(ResourceSet&&) = default;
		ResourceSet& operator=(const ResourceSet&) = delete;
		ResourceSet& operator=(ResourceSet&&) = default;
	};
}

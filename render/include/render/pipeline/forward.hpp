#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <vulkan/vulkan_raii.hpp>

#include "model/mesh.hpp"
#include "render/interface/camera.hpp"
#include "render/interface/direct-light.hpp"
#include "render/interface/indirect-drawcall.hpp"
#include "render/model/model.hpp"
#include "render/resource/forward-rendering.hpp"
#include "render/resource/host.hpp"
#include "render/resource/indirect.hpp"
#include "render/util/per-render-state.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/interface/common-context.hpp"

namespace render
{
	///
	/// @brief Forward rendering pipeline
	/// @details
	/// - Takes the indirect drawcalls and render to a HDR target
	/// - Supports 4 material variants, where BLEND is currently rendered as MASK
	///
	class ForwardPipeline
	{
	  public:

		class ResourceSet;

		///
		/// @brief Create a forward rendering pipeline
		///
		/// @param context Vulkan context
		/// @param material_layout Model material layout
		/// @param color_format Color target format
		/// @param depth_format Depth target format
		/// @return Created forward rendering pipeline, or error if creation failed
		///
		[[nodiscard]]
		static std::expected<ForwardPipeline, Error> create(
			const vulkan::DeviceContext& context,
			const render::MaterialLayout& material_layout,
			vk::Format color_format,
			vk::Format depth_format
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
			const vulkan::DeviceContext& context,
			uint32_t count
		) const noexcept;

		///
		/// @brief Render the scene using the forward rendering pipeline
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
			const vulkan::DeviceContext& context,
			const vk::raii::PipelineLayout& pipeline_layout,
			const vk::raii::ShaderModule& shader_module,
			vk::Format color_format,
			vk::Format depth_format,
			bool alpha_mask_enabled,
			bool double_sided
		) noexcept;

		vk::raii::DescriptorSetLayout data_descriptor_set_layout;
		vk::raii::PipelineLayout pipeline_layout;
		PerRenderState<vk::raii::Pipeline> pipelines;

		explicit ForwardPipeline(
			vk::raii::DescriptorSetLayout data_descriptor_set_layout,
			vk::raii::PipelineLayout pipeline_layout,
			PerRenderState<vk::raii::Pipeline> pipelines
		) :
			data_descriptor_set_layout(std::move(data_descriptor_set_layout)),
			pipeline_layout(std::move(pipeline_layout)),
			pipelines(std::move(pipelines))
		{}

	  public:

		ForwardPipeline(const ForwardPipeline&) = delete;
		ForwardPipeline(ForwardPipeline&&) = default;
		ForwardPipeline& operator=(const ForwardPipeline&) = delete;
		ForwardPipeline& operator=(ForwardPipeline&&) = default;
	};

	///
	/// @brief Resource set for forward pipeline
	///
	class ForwardPipeline::ResourceSet
	{
	  public:

		///
		/// @brief Update the resource set
		///
		/// @param context Vulkan context
		/// @param model Model instance
		/// @param camera_param Camera parameter buffer
		/// @param primary_light_param  Primary light parameter buffer
		/// @param host_drawcall Host drawcall resource
		/// @param indirect_resource Indirect drawcall resource
		/// @param forward_resource Forward rendering resource containing the render targets
		/// @param extent Rendering extent
		///
		void update(
			const vulkan::DeviceContext& context,
			const Model& model,
			const vulkan::ElementBufferRef<Camera>& camera_param,
			const vulkan::ElementBufferRef<DirectLight>& primary_light_param,
			const HostDrawcallResource& host_drawcall,
			const IndirectResource& indirect_resource,
			const ForwardRenderResource& forward_resource,
			glm::u32vec2 extent
		) noexcept;

	  private:

		std::shared_ptr<vk::raii::DescriptorPool> descriptor_pool;

		vk::DescriptorSet material_descriptor_set = nullptr;
		PerRenderState<vk::raii::DescriptorSet> data_descriptor_set;

		vulkan::ArrayBufferRef<model::FullVertex> vertex_buffer;
		vulkan::ArrayBufferRef<uint32_t> index_buffer;
		PerRenderState<vulkan::ArrayBufferRef<IndirectDrawcall>> indirect_buffers = {
			.opaque_single_sided = {},
			.opaque_double_sided = {},
			.masked_single_sided = {},
			.masked_double_sided = {}
		};

		glm::u32vec2 rendering_extent = {0, 0};
		vk::Image color_target = nullptr, depth_target = nullptr;
		vk::ImageView color_target_view = nullptr, depth_target_view = nullptr;

		explicit ResourceSet(
			std::shared_ptr<vk::raii::DescriptorPool> descriptor_pool,
			PerRenderState<vk::raii::DescriptorSet> data_descriptor_set
		) :
			descriptor_pool(std::move(descriptor_pool)),
			data_descriptor_set(std::move(data_descriptor_set))
		{}

		friend class ForwardPipeline;

	  public:

		ResourceSet(const ResourceSet&) = delete;
		ResourceSet(ResourceSet&&) = default;
		ResourceSet& operator=(const ResourceSet&) = delete;
		ResourceSet& operator=(ResourceSet&&) = default;
	};
}

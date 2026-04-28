#pragma once

#include <cstdint>
#include <expected>
#include <memory>
#include <vulkan/vulkan_raii.hpp>

#include "render/interface/indirect-drawcall.hpp"
#include "render/model/model.hpp"
#include "render/resource/host.hpp"
#include "render/resource/indirect.hpp"
#include "render/util/per-render-state.hpp"
#include "vulkan/interface/common-context.hpp"

namespace render
{
	///
	/// @brief Indirect pipeline, computes the indirect drawcalls
	/// @details
	/// - Takes the drawcall data and outputs the indirect drawcall buffers
	/// - Supports 4 material variants, where BLEND is currently rendered as MASK
	///
	class IndirectPipeline
	{
	  public:

		class ResourceSet;

		///
		/// @brief Create an indirect pipeline
		///
		/// @param context Vulkan context
		/// @return Created indirect pipeline, or error
		///
		[[nodiscard]]
		static std::expected<IndirectPipeline, Error> create(const vulkan::DeviceContext& context) noexcept;

		///
		/// @brief Create a number of resource sets
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
		/// @brief Execute the compute pass to generate indirect drawcalls
		///
		/// @param command_buffer Command buffer
		/// @param resource_set Resource set to bind for the compute
		///
		void compute(
			const vk::raii::CommandBuffer& command_buffer,
			const ResourceSet& resource_set
		) const noexcept;

	  private:

		using PushConstant = uint32_t;
		static constexpr uint32_t WORKGROUP_SIZE = 64;

		vk::raii::DescriptorSetLayout descriptor_set_layout;
		vk::raii::PipelineLayout pipeline_layout;
		vk::raii::Pipeline pipeline;

		explicit IndirectPipeline(
			vk::raii::DescriptorSetLayout descriptor_set_layout,
			vk::raii::PipelineLayout pipeline_layout,
			vk::raii::Pipeline pipeline
		) :
			descriptor_set_layout(std::move(descriptor_set_layout)),
			pipeline_layout(std::move(pipeline_layout)),
			pipeline(std::move(pipeline))
		{}

	  public:

		IndirectPipeline(const IndirectPipeline&) = delete;
		IndirectPipeline(IndirectPipeline&&) = default;
		IndirectPipeline& operator=(const IndirectPipeline&) = delete;
		IndirectPipeline& operator=(IndirectPipeline&&) = default;
	};

	///
	/// @brief Resource set for indirect pipeline
	///
	class IndirectPipeline::ResourceSet
	{
	  public:

		///
		/// @brief Update the indirect resource pipeline
		///
		/// @param context Vulkan context
		/// @param model Model instance
		/// @param drawcall_resource Host drawcall resource
		/// @param indirect_resource Indirect drawcall resource
		///
		void update(
			const vulkan::DeviceContext& context,
			const Model& model,
			const HostDrawcallResource& drawcall_resource,
			const IndirectResource& indirect_resource
		) noexcept;

	  private:

		std::shared_ptr<vk::raii::DescriptorPool> descriptor_pool;
		PerRenderState<vk::raii::DescriptorSet> descriptor_sets;
		PerRenderState<vulkan::ArrayBufferRef<IndirectDrawcall>> indirect_buffers = {};

		friend class IndirectPipeline;

		explicit ResourceSet(
			std::shared_ptr<vk::raii::DescriptorPool> descriptor_pool,
			PerRenderState<vk::raii::DescriptorSet> descriptor_sets
		) noexcept :
			descriptor_pool(std::move(descriptor_pool)),
			descriptor_sets(std::move(descriptor_sets))
		{}

	  public:

		ResourceSet(const ResourceSet&) = delete;
		ResourceSet(ResourceSet&&) = default;
		ResourceSet& operator=(const ResourceSet&) = delete;
		ResourceSet& operator=(ResourceSet&&) = default;
	};
}

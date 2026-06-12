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

#include "common/number-literals.hpp"
#include "common/util/error.hpp"
#include "render/resource/deferred.hpp"
#include "vulkan/interface/context.hpp"

namespace render
{
	///
	/// @brief Deferred attachment downsampling pipeline
	/// @note Takes the top-left pixel every 2x2 tile in the deferred attachments
	///
	class DownsamplePipeline
	{
	  public:

		class ResourceSet;

		///
		/// @brief Create a downsample pipeline
		///
		/// @param context Vulkan context
		/// @return Created downsample pipeline, or error if creation failed
		///
		[[nodiscard]]
		static std::expected<DownsamplePipeline, Error> create(const vulkan::Context& context) noexcept;

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
		/// @brief Downsample the attachment
		///
		/// @param command_buffer Command buffer
		/// @param resource_set Resource set
		///
		void downsample(
			const vk::raii::CommandBuffer& command_buffer,
			const ResourceSet& resource_set
		) const noexcept;

	  private:

		using PushConstant = glm::u32vec2;
		static constexpr auto BLOCK_SIZE = 16_u32;

		vk::raii::DescriptorSetLayout descriptor_set_layout;
		vk::raii::PipelineLayout pipeline_layout;
		vk::raii::Pipeline pipeline;

		vk::raii::Sampler texture_sampler;

		explicit DownsamplePipeline(
			vk::raii::DescriptorSetLayout descriptor_set_layout,
			vk::raii::PipelineLayout pipeline_layout,
			vk::raii::Pipeline pipeline,
			vk::raii::Sampler texture_sampler
		) :
			descriptor_set_layout(std::move(descriptor_set_layout)),
			pipeline_layout(std::move(pipeline_layout)),
			pipeline(std::move(pipeline)),
			texture_sampler(std::move(texture_sampler))
		{}

	  public:

		DownsamplePipeline(const DownsamplePipeline&) = delete;
		DownsamplePipeline(DownsamplePipeline&&) = default;
		DownsamplePipeline& operator=(const DownsamplePipeline&) = delete;
		DownsamplePipeline& operator=(DownsamplePipeline&&) = default;
	};

	///
	/// @brief Resource set for downsampling pipeline
	///
	class DownsamplePipeline::ResourceSet
	{
	  public:

		///
		/// @brief Update resource set
		///
		/// @param context Vulkan context
		/// @param deferred Deferred attachment
		/// @param halfres_deferred Half-resolution deferred attachment
		///
		void update(
			const vulkan::Context& context,
			DeferredAttachment::View deferred,
			HalfDeferredAttachment::View halfres_deferred
		) noexcept;

	  private:

		std::shared_ptr<vk::raii::DescriptorPool> descriptor_pool;
		vk::raii::DescriptorSet descriptor_set;

		vk::Sampler texture_sampler;

		std::optional<HalfDeferredAttachment::View> attachment = std::nullopt;

		explicit ResourceSet(
			std::shared_ptr<vk::raii::DescriptorPool> descriptor_pool,
			vk::raii::DescriptorSet descriptor_set,
			vk::Sampler texture_sampler
		) :
			descriptor_pool(std::move(descriptor_pool)),
			descriptor_set(std::move(descriptor_set)),
			texture_sampler(texture_sampler)
		{}

		friend class DownsamplePipeline;

	  public:

		ResourceSet(const ResourceSet&) = delete;
		ResourceSet(ResourceSet&&) = default;
		ResourceSet& operator=(const ResourceSet&) = delete;
		ResourceSet& operator=(ResourceSet&&) = default;
	};
}

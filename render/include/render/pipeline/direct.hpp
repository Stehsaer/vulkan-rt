#pragma once

#include "common/util/error.hpp"
#include "render/interface/camera.hpp"
#include "render/interface/direct-light.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/hdr.hpp"
#include "render/resource/shadow.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/interface/context.hpp"

#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	///
	/// @brief Direct-light pipeline
	/// @details
	/// - Takes the deferred attachments and calculate lighting
	/// - Lighting result is added to the HDR attachment
	///
	class DirectLightingPipeline
	{
	  public:

		class ResourceSet;

		///
		/// @brief Create a direct lighting pipeline
		///
		/// @param context Vulkan context
		/// @return Created pipeline or error
		///
		[[nodiscard]]
		static std::expected<DirectLightingPipeline, Error> create(const vulkan::Context& context) noexcept;

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
		/// @brief Perform lighting
		/// @note The procedure is designed to operate in an externally-managed dynamic rendering session. It
		/// do neither begin or end the dynamic rendering session, nor perform any synchronization.
		///
		/// @param command_buffer Command buffer
		/// @param resource_set Resource set
		///
		void render(
			const vk::raii::CommandBuffer& command_buffer,
			const ResourceSet& resource_set
		) const noexcept;

	  private:

		vk::raii::DescriptorSetLayout descriptor_set_layout;
		vk::raii::PipelineLayout pipeline_layout;
		vk::raii::Pipeline pipeline;
		vk::raii::Sampler sampler;

		explicit DirectLightingPipeline(
			vk::raii::DescriptorSetLayout descriptor_set_layout,
			vk::raii::PipelineLayout pipeline_layout,
			vk::raii::Pipeline pipeline,
			vk::raii::Sampler sampler
		) :
			descriptor_set_layout(std::move(descriptor_set_layout)),
			pipeline_layout(std::move(pipeline_layout)),
			pipeline(std::move(pipeline)),
			sampler(std::move(sampler))
		{}

	  public:

		DirectLightingPipeline(const DirectLightingPipeline&) = delete;
		DirectLightingPipeline(DirectLightingPipeline&&) = default;
		DirectLightingPipeline& operator=(const DirectLightingPipeline&) = delete;
		DirectLightingPipeline& operator=(DirectLightingPipeline&&) = default;
	};

	///
	/// @brief Resource set for direct lighting pipeline
	///
	class DirectLightingPipeline::ResourceSet
	{
	  public:

		///
		/// @brief Update resource set
		///
		/// @param context Vulkan context
		/// @param deferred Deferred attachment
		/// @param hdr HDR attachment
		/// @param shadow Shadow attachment
		/// @param camera Camera parameter buffer
		/// @param direct_light Direct light buffer
		///
		void update(
			const vulkan::Context& context,
			DeferredAttachment::View deferred,
			HdrAttachment::View hdr,
			ShadowAttachment::View shadow,
			vulkan::ElementBufferRef<Camera> camera,
			vulkan::ElementBufferRef<DirectLight> direct_light
		) noexcept;

	  private:

		std::shared_ptr<vk::raii::DescriptorPool> pool;
		vk::raii::DescriptorSet set;

		vk::Sampler sampler;

		struct Resource
		{
			HdrAttachment::View hdr;
		};

		std::optional<Resource> resource = std::nullopt;

		const Resource* operator->() const noexcept { return resource.operator->(); }

		explicit ResourceSet(
			std::shared_ptr<vk::raii::DescriptorPool> pool,
			vk::raii::DescriptorSet set,
			vk::Sampler sampler
		) :
			pool(std::move(pool)),
			set(std::move(set)),
			sampler(sampler)
		{}

		friend class DirectLightingPipeline;

	  public:

		ResourceSet(const ResourceSet&) = delete;
		ResourceSet(ResourceSet&&) = default;
		ResourceSet& operator=(const ResourceSet&) = delete;
		ResourceSet& operator=(ResourceSet&&) = default;
	};
}

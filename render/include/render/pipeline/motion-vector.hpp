#pragma once

#include "common/number-literals.hpp"
#include "common/util/error.hpp"
#include "render/interface/camera.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/motion-vector.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"

#include <cstdint>
#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	///
	/// @brief Motion vector generation pipeline
	///
	class MotionVectorPipeline
	{
	  public:

		class ResourceSet;

		///
		/// @brief Create a motion-vector pipeline
		///
		/// @param context Vulkan context
		/// @return Created pipeline or error
		///
		[[nodiscard]]
		static std::expected<MotionVectorPipeline, Error> create(const vulkan::Context& context) noexcept;

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
		/// @brief Compute and generate motion vector
		///
		/// @param command_buffer Command buffer
		/// @param resource_set Resource set to compute with
		///
		void compute(
			const vk::raii::CommandBuffer& command_buffer,
			const ResourceSet& resource_set
		) const noexcept;

	  private:

		struct PushConstant
		{
			glm::u32vec2 half_size;
			glm::u32vec2 full_size;
		};

		static constexpr auto BLOCK_SIZE = 16_u32;

		vk::raii::DescriptorSetLayout set_layout;
		vk::raii::PipelineLayout pipeline_layout;
		vk::raii::Pipeline pipeline;

		vk::raii::Sampler sampler;

		explicit MotionVectorPipeline(
			vk::raii::DescriptorSetLayout set_layout,
			vk::raii::PipelineLayout pipeline_layout,
			vk::raii::Pipeline pipeline,
			vk::raii::Sampler sampler
		) :
			set_layout(std::move(set_layout)),
			pipeline_layout(std::move(pipeline_layout)),
			pipeline(std::move(pipeline)),
			sampler(std::move(sampler))
		{}

	  public:

		MotionVectorPipeline(const MotionVectorPipeline&) = delete;
		MotionVectorPipeline(MotionVectorPipeline&&) = default;
		MotionVectorPipeline& operator=(const MotionVectorPipeline&) = delete;
		MotionVectorPipeline& operator=(MotionVectorPipeline&&) = default;
	};

	///
	/// @brief Resource set for motion vector pipeline
	///
	class MotionVectorPipeline::ResourceSet
	{
	  public:

		///
		/// @brief Update resource set
		///
		/// @param context Vulkan context
		/// @param attachment Motion vector attachment
		/// @param gbuffer Gbuffer attachment
		/// @param prev_gbuffer Previous-frame gbuffer
		/// @param camera Camera parameter buffer
		///
		void update(
			const vulkan::Context& context,
			MotionVectorAttachment::View attachment,
			HalfDeferredAttachment::View gbuffer,
			HalfDeferredAttachment::View prev_gbuffer,
			vulkan::ElementBufferRef<Camera> camera
		) noexcept;

	  private:

		std::shared_ptr<vk::raii::DescriptorPool> pool;
		vk::raii::DescriptorSet set;

		vk::Sampler sampler;

		struct Resource
		{
			glm::u32vec2 full_size;
			glm::u32vec2 half_size;
			vulkan::AttachmentView motion_vector;
		};

		std::optional<Resource> resource = std::nullopt;

		auto operator->() const noexcept { return resource.operator->(); }

		explicit ResourceSet(
			std::shared_ptr<vk::raii::DescriptorPool> pool,
			vk::raii::DescriptorSet set,
			vk::Sampler sampler
		) :
			pool(std::move(pool)),
			set(std::move(set)),
			sampler(sampler)
		{}

		friend MotionVectorPipeline;

	  public:

		ResourceSet(const ResourceSet&) = delete;
		ResourceSet(ResourceSet&&) = default;
		ResourceSet& operator=(const ResourceSet&) = delete;
		ResourceSet& operator=(ResourceSet&&) = default;
	};
}

#pragma once

#include "common/number-literals.hpp"
#include "common/util/error.hpp"
#include "render/resource/motion-vector.hpp"
#include "render/resource/shadow.hpp"
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

namespace render::shadow
{
	///
	/// @brief Temporal accumulation/denoise pipeline
	///
	/// @details
	/// Reprojects history frame and accumulates. History frame is clamped using spatial mean and variance
	/// from @ref SpatialVariancePipeline.
	///
	/// #### Input
	///
	/// - Initial sampled visibility
	/// - History frame
	/// - Spatial mean
	/// - Spatial stddev
	/// - Motion vector
	///
	/// #### Output
	///
	/// - New history frame (for use in next frame)
	/// - Denoise texture (Alice)
	///
	class TemporalDenoisePipeline
	{
	  public:

		class ResourceSet;

		///
		/// @brief Create temporal denoising pipeline
		///
		/// @param context Vulkan context
		/// @return Created pipeline or error
		///
		[[nodiscard]]
		static std::expected<TemporalDenoisePipeline, Error> create(const vulkan::Context& context) noexcept;

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
		/// @brief Run denoise pass
		///
		/// @param command_buffer Command buffer
		/// @param resource_set Resource set to use
		///
		void denoise(
			const vk::raii::CommandBuffer& command_buffer,
			const ResourceSet& resource_set
		) const noexcept;

	  private:

		static constexpr auto BLOCK_SIZE = 16_u32;

		vk::raii::DescriptorSetLayout set_layout;
		vk::raii::PipelineLayout pipeline_layout;
		vk::raii::Pipeline pipeline;

		vk::raii::Sampler sampler;

		explicit TemporalDenoisePipeline(
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

		TemporalDenoisePipeline(const TemporalDenoisePipeline&) = delete;
		TemporalDenoisePipeline(TemporalDenoisePipeline&&) = default;
		TemporalDenoisePipeline& operator=(const TemporalDenoisePipeline&) = delete;
		TemporalDenoisePipeline& operator=(TemporalDenoisePipeline&&) = default;
	};

	class TemporalDenoisePipeline::ResourceSet
	{
	  public:

		///
		/// @brief Update resource set
		///
		/// @param context Vulkan context
		/// @param shadow Shadow attachment
		/// @param prev_shadow Previous-frame shadow attachment
		/// @param motion_vector Motion-vector attachment
		///
		void update(
			const vulkan::Context& context,
			ShadowAttachment::View shadow,
			ShadowAttachment::View prev_shadow,
			MotionVectorAttachment::View motion_vector
		) noexcept;

	  private:

		std::shared_ptr<vk::raii::DescriptorPool> pool;
		vk::raii::DescriptorSet set;
		vk::Sampler sampler;

		struct Resource
		{
			glm::u32vec2 half_extent;
			vulkan::AttachmentView curr_history;
			vulkan::AttachmentView denoise_alice;
		};

		std::optional<Resource> resource = std::nullopt;

		auto operator->() const noexcept { return resource.operator->(); }

		ResourceSet(
			std::shared_ptr<vk::raii::DescriptorPool> pool,
			vk::raii::DescriptorSet set,
			vk::Sampler sampler
		) :
			pool(std::move(pool)),
			set(std::move(set)),
			sampler(sampler)
		{}

		friend class TemporalDenoisePipeline;

	  public:

		ResourceSet(const ResourceSet&) = delete;
		ResourceSet(ResourceSet&&) = default;
		ResourceSet& operator=(const ResourceSet&) = delete;
		ResourceSet& operator=(ResourceSet&&) = default;
	};
}

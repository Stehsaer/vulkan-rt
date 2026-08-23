#pragma once

#include "common/number-literals.hpp"
#include "common/util/error.hpp"
#include "render/interface/camera.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/shadow.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/util/compute-pipeline.hpp"
#include "vulkan/util/trivial-descriptor-set.hpp"

#include <cstdint>
#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render::shadow
{
	///
	/// @brief Spatial variance compute pipeline
	///
	/// @details
	/// Computes spatial mean and stddev (guided by half-res GBuffer) on current frame. Stddev is further
	/// filtered to stay conservative.
	///
	/// #### Input
	/// - Half-resolution gbuffer
	/// - Initial sampled visibility
	///
	/// #### Output
	/// - Spatial mean
	/// - Spatial stddev
	///
	class SpatialVariancePipeline
	{
	  public:

		class ResourceSet;

		///
		/// @brief Create the pipeline instance
		///
		/// @param context Vulkan context
		/// @return Created pipeline or error
		///
		[[nodiscard]]
		static std::expected<SpatialVariancePipeline, Error> create(const vulkan::Context& context) noexcept;

		///
		/// @brief Create a given number of resource sets
		///
		/// @param context Vulkan context
		/// @param count Number of resource set to create
		/// @return Created resource sets or error
		///
		[[nodiscard]]
		std::expected<std::vector<ResourceSet>, Error> create_resource_sets(
			const vulkan::Context& context,
			uint32_t count
		) const noexcept;

		///
		/// @brief Generate spatial variance
		///
		/// @param command_buffer Command buffer
		/// @param resource_set Resource set to use
		///
		void generate(
			const vk::raii::CommandBuffer& command_buffer,
			const ResourceSet& resource_set
		) const noexcept;

	  private:

		static constexpr auto BLOCK_SIZE = 16_u32;

		struct Extent
		{
			glm::u32vec2 half;
			glm::u32vec2 full;
		};

		struct ComputeInput : public vulkan::trivset::LayoutBase
		{
			CombinedImageSampler input_tex;
			StorageImage mean_tex;
			StorageImage stddev_tex;
			CombinedImageSampler depth_tex;
			CombinedImageSampler normal_tex;
			UniformBuffer camera;

			static constexpr auto STAGE = vk::ShaderStageFlagBits::eCompute;
			static constexpr auto SLOT_LIST = std::make_tuple(
				&ComputeInput::input_tex,
				&ComputeInput::mean_tex,
				&ComputeInput::stddev_tex,
				&ComputeInput::depth_tex,
				&ComputeInput::normal_tex,
				&ComputeInput::camera
			);
		};

		struct FilterInput : public vulkan::trivset::LayoutBase
		{
			CombinedImageSampler input_stddev_tex;
			StorageImage output_stddev_tex;
			CombinedImageSampler depth_tex;
			CombinedImageSampler normal_tex;
			UniformBuffer camera;

			static constexpr auto STAGE = vk::ShaderStageFlagBits::eCompute;
			static constexpr auto SLOT_LIST = std::make_tuple(
				&FilterInput::input_stddev_tex,
				&FilterInput::output_stddev_tex,
				&FilterInput::depth_tex,
				&FilterInput::normal_tex,
				&FilterInput::camera
			);
		};

		using ComputePipeline = vulkan::ComputePipeline<Extent, {16, 16, 1}>;
		using FilterPipeline = vulkan::ComputePipeline<Extent, {16, 16, 1}>;

		vulkan::trivset::Layout<ComputeInput> compute_set_layout;
		ComputePipeline compute_pipeline;

		vulkan::trivset::Layout<FilterInput> filter_set_layout;
		FilterPipeline filter_pipeline;

		vk::raii::Sampler sampler;

		explicit SpatialVariancePipeline(
			vulkan::trivset::Layout<ComputeInput> compute_set_layout,
			ComputePipeline compute_pipeline,
			vulkan::trivset::Layout<FilterInput> filter_set_layout,
			FilterPipeline filter_pipeline,
			vk::raii::Sampler sampler
		) :
			compute_set_layout(std::move(compute_set_layout)),
			compute_pipeline(std::move(compute_pipeline)),
			filter_set_layout(std::move(filter_set_layout)),
			filter_pipeline(std::move(filter_pipeline)),
			sampler(std::move(sampler))
		{}

		// Compute subpass
		void compute(
			const vk::raii::CommandBuffer& command_buffer,
			const ResourceSet& resource_set
		) const noexcept;

		// Filter subpass
		void filter(
			const vk::raii::CommandBuffer& command_buffer,
			const ResourceSet& resource_set
		) const noexcept;

	  public:

		SpatialVariancePipeline(const SpatialVariancePipeline&) = delete;
		SpatialVariancePipeline(SpatialVariancePipeline&&) = default;
		SpatialVariancePipeline& operator=(const SpatialVariancePipeline&) = delete;
		SpatialVariancePipeline& operator=(SpatialVariancePipeline&&) = default;
	};

	///
	/// @brief Resource set for spatial variance pipeline
	///
	class SpatialVariancePipeline::ResourceSet
	{
	  public:

		void update(
			const vulkan::Context& context,
			HalfDeferredAttachment::View half_deferred,
			ShadowAttachment::View shadow,
			vulkan::ElementBufferRef<Camera> camera
		) noexcept;

	  private:

		vulkan::trivset::Set<ComputeInput> compute_set;
		vulkan::trivset::Set<FilterInput> filter_set;
		vk::Sampler sampler;

		struct Resource
		{
			glm::u32vec2 half_extent;
			glm::u32vec2 full_extent;
			vulkan::AttachmentView spatial_mean;
			vulkan::AttachmentView spatial_stddev;
			vulkan::AttachmentView filtered_spatial_stddev;
		};

		std::optional<Resource> resource = std::nullopt;

		auto operator->() const noexcept { return resource.operator->(); }

		ResourceSet(
			vulkan::trivset::Set<ComputeInput> compute_set,
			vulkan::trivset::Set<FilterInput> filter_set,
			vk::Sampler sampler
		) :
			compute_set(std::move(compute_set)),
			filter_set(std::move(filter_set)),
			sampler(sampler)
		{}

		friend SpatialVariancePipeline;

	  public:

		ResourceSet(const ResourceSet&) = delete;
		ResourceSet(ResourceSet&&) = default;
		ResourceSet& operator=(const ResourceSet&) = delete;
		ResourceSet& operator=(ResourceSet&&) = default;
	};
}

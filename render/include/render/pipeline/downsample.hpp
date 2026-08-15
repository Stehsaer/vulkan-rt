#pragma once

#include <cstdint>
#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "common/number-literals.hpp"
#include "common/util/error.hpp"
#include "render/resource/deferred.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/util/trivial-descriptor-set.hpp"

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

		struct Input : public vulkan::trivset::LayoutBase
		{
			CombinedImageSampler full_albedo_tex;
			CombinedImageSampler full_normal_tex;
			CombinedImageSampler full_geom_normal_tex;
			CombinedImageSampler full_smooth_normal_tex;
			CombinedImageSampler full_pbr_tex;
			CombinedImageSampler full_depth_tex;

			StorageImage half_albedo_tex;
			StorageImage half_normal_tex;
			StorageImage half_geom_normal_tex;
			StorageImage half_smooth_normal_tex;
			StorageImage half_pbr_tex;
			StorageImage half_depth_tex;

			static constexpr auto STAGE = vk::ShaderStageFlagBits::eCompute;
			static constexpr auto SLOT_LIST = std::make_tuple(
				&Input::full_albedo_tex,
				&Input::full_normal_tex,
				&Input::full_geom_normal_tex,
				&Input::full_smooth_normal_tex,
				&Input::full_pbr_tex,
				&Input::full_depth_tex,
				&Input::half_albedo_tex,
				&Input::half_normal_tex,
				&Input::half_geom_normal_tex,
				&Input::half_smooth_normal_tex,
				&Input::half_pbr_tex,
				&Input::half_depth_tex
			);
		};

		vulkan::trivset::Layout<Input> descriptor_set_layout;
		vk::raii::PipelineLayout pipeline_layout;
		vk::raii::Pipeline pipeline;

		vk::raii::Sampler texture_sampler;

		explicit DownsamplePipeline(
			vulkan::trivset::Layout<Input> descriptor_set_layout,
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

		vulkan::trivset::Set<Input> descriptor_set;
		vk::Sampler texture_sampler;

		std::optional<HalfDeferredAttachment::View> attachment = std::nullopt;

		explicit ResourceSet(vulkan::trivset::Set<Input> descriptor_set, vk::Sampler texture_sampler) :
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

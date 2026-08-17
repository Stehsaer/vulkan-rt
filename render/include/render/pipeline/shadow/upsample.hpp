#pragma once

#include "common/number-literals.hpp"
#include "common/util/error.hpp"
#include "render/interface/camera.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/shadow.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"
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
	/// @brief Shadow edge-aware upsampling pipeline
	///
	/// @details
	/// Upsamples denoised shadow visibility buffer to full-resolution. Two subpasses are used:
	/// - Pass 1: compute visibility of 8 neighbor pixels around the valid sample and stored as an 8-bit mask
	/// - Pass 2: uses the mask and blend valid samples to fill-up 3/4 of full-res pixels
	///
	/// #### Extra Input
	/// - Full-resolution gbuffer
	/// - Camera parameters
	///
	class UpsamplePipeline
	{
	  public:

		class ResourceSet;

		///
		/// @brief Create an upsampling pipeline
		///
		/// @param context Vulkan context
		/// @return Created pipeline or error
		///
		[[nodiscard]]
		static std::expected<UpsamplePipeline, Error> create(const vulkan::Context& context) noexcept;

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
		/// @brief Run the upsampling pass
		///
		/// @param command_buffer Command buffer
		/// @param resource_set Resource set to use
		///
		void upsample(
			const vk::raii::CommandBuffer& command_buffer,
			const ResourceSet& resource_set
		) const noexcept;

	  private:

		struct Resolution
		{
			glm::u32vec2 half;
			glm::u32vec2 full;
		};

		struct MaskInput : public vulkan::trivset::LayoutBase
		{
			CombinedImageSampler full_depth_tex;
			CombinedImageSampler full_normal_tex;
			UniformBuffer camera;
			StorageImage visibility_mask;

			static constexpr auto STAGE = vk::ShaderStageFlagBits::eCompute;
			static constexpr auto SLOT_LIST = std::make_tuple(
				&MaskInput::full_depth_tex,
				&MaskInput::full_normal_tex,
				&MaskInput::camera,
				&MaskInput::visibility_mask
			);
		};

		struct GenInput : public vulkan::trivset::LayoutBase
		{
			CombinedImageSampler half_shadow;
			StorageImage full_shadow;
			CombinedImageSampler visibility_mask;

			static constexpr auto STAGE = vk::ShaderStageFlagBits::eCompute;
			static constexpr auto SLOT_LIST =
				std::make_tuple(&GenInput::half_shadow, &GenInput::full_shadow, &GenInput::visibility_mask);
		};

		static constexpr auto BLOCK_SIZE = 16_u32;

		vulkan::trivset::Layout<MaskInput> mask_set_layout;
		vulkan::trivset::Layout<GenInput> gen_set_layout;
		vk::raii::PipelineLayout mask_pipeline_layout;
		vk::raii::PipelineLayout gen_pipeline_layout;
		vk::raii::Pipeline mask_pipeline;
		vk::raii::Pipeline gen_pipeline;
		vk::raii::Sampler sampler;

		explicit UpsamplePipeline(
			vulkan::trivset::Layout<MaskInput> mask_set_layout,
			vulkan::trivset::Layout<GenInput> gen_set_layout,
			vk::raii::PipelineLayout mask_pipeline_layout,
			vk::raii::PipelineLayout gen_pipeline_layout,
			vk::raii::Pipeline mask_pipeline,
			vk::raii::Pipeline gen_pipeline,
			vk::raii::Sampler sampler
		) :
			mask_set_layout(std::move(mask_set_layout)),
			gen_set_layout(std::move(gen_set_layout)),
			mask_pipeline_layout(std::move(mask_pipeline_layout)),
			gen_pipeline_layout(std::move(gen_pipeline_layout)),
			mask_pipeline(std::move(mask_pipeline)),
			gen_pipeline(std::move(gen_pipeline)),
			sampler(std::move(sampler))
		{}

	  public:

		UpsamplePipeline(const UpsamplePipeline&) = delete;
		UpsamplePipeline(UpsamplePipeline&&) = default;
		UpsamplePipeline& operator=(const UpsamplePipeline&) = delete;
		UpsamplePipeline& operator=(UpsamplePipeline&&) = default;
	};

	///
	/// @brief Resource set for upsampling pipeline
	///
	class UpsamplePipeline::ResourceSet
	{
	  public:

		///
		/// @brief Update resource set
		///
		/// @param context Vulkan context
		/// @param gbuffer Full-res gbuffer attachments
		/// @param attachment Shadow attachments
		/// @param camera Camera
		///
		void update(
			const vulkan::Context& context,
			DeferredAttachment::View gbuffer,
			ShadowAttachment::View attachment,
			vulkan::ElementBufferRef<Camera> camera
		) noexcept;

	  private:

		struct Resource
		{
			glm::u32vec2 half_extent;
			glm::u32vec2 full_extent;
			vulkan::AttachmentView bitmask;
			vulkan::AttachmentView visibility;
		};

		vulkan::trivset::Set<MaskInput> mask_set;
		vulkan::trivset::Set<GenInput> gen_set;
		vk::Sampler sampler;

		std::optional<Resource> resource = std::nullopt;

		auto operator->() const noexcept { return resource.operator->(); }

		friend class UpsamplePipeline;

		ResourceSet(
			vulkan::trivset::Set<MaskInput> mask_set,
			vulkan::trivset::Set<GenInput> gen_set,
			vk::Sampler sampler
		) :
			mask_set(std::move(mask_set)),
			gen_set(std::move(gen_set)),
			sampler(sampler)
		{}

	  public:

		ResourceSet(const ResourceSet&) = delete;
		ResourceSet(ResourceSet&&) = default;
		ResourceSet& operator=(const ResourceSet&) = delete;
		ResourceSet& operator=(ResourceSet&&) = default;
	};
}

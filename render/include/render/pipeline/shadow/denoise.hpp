#pragma once

#include "common/number-literals.hpp"
#include "common/util/error.hpp"
#include "render/interface/camera.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/shadow.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"

#include <array>
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
	/// @brief Shadow denoising pipeline
	///
	///
	class DenoisePipeline
	{
	  public:

		static constexpr auto FILTER_PASSES = 3zu;
		static_assert(FILTER_PASSES % 2 == 1);  // Odd number of passes are required

		class ResourceSet;

		///
		/// @brief Create the denoising pipeline
		///
		/// @param context Vulkan context
		/// @return Created pipeline or error
		///
		[[nodiscard]]
		static std::expected<DenoisePipeline, Error> create(const vulkan::Context& context) noexcept;

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
		/// @brief Run the denoise passes
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

		struct PushConstant
		{
			glm::u32vec2 half_size;
			uint32_t stride;
		};

		vk::raii::DescriptorSetLayout input_layout;
		vk::raii::PipelineLayout pipeline_layout;
		vk::raii::Pipeline pipeline;

		vk::raii::Sampler sampler;

		explicit DenoisePipeline(
			vk::raii::DescriptorSetLayout input_layout,
			vk::raii::PipelineLayout pipeline_layout,
			vk::raii::Pipeline pipeline,
			vk::raii::Sampler sampler
		) :
			input_layout(std::move(input_layout)),
			pipeline_layout(std::move(pipeline_layout)),
			pipeline(std::move(pipeline)),
			sampler(std::move(sampler))
		{}

	  public:

		DenoisePipeline(const DenoisePipeline&) = delete;
		DenoisePipeline(DenoisePipeline&&) = default;
		DenoisePipeline& operator=(const DenoisePipeline&) = delete;
		DenoisePipeline& operator=(DenoisePipeline&&) = default;
	};

	///
	/// @brief Resource set for denoise pipeline
	///
	class DenoisePipeline::ResourceSet
	{
	  public:

		///
		/// @brief Update resource set
		///
		/// @param context Vulkan context
		/// @param camera Camera
		/// @param half_gbuffer Half-res gbuffer attachments
		/// @param shadow Shadow attachments
		///
		void update(
			const vulkan::Context& context,
			vulkan::ElementBufferRef<Camera> camera,
			HalfDeferredAttachment::View half_gbuffer,
			ShadowAttachment::View shadow
		) noexcept;

	  private:

		std::shared_ptr<vk::raii::DescriptorPool> pool;
		std::array<std::unique_ptr<vk::raii::DescriptorSet>, FILTER_PASSES> sets;
		vk::Sampler sampler;

		struct Resource
		{
			glm::u32vec2 half_extent;
			vulkan::AttachmentView init_sample;
			vulkan::AttachmentView denoise_imm;
			vulkan::AttachmentView denoise_final;
		};

		std::optional<Resource> resource = std::nullopt;

		auto operator->() const noexcept { return resource.operator->(); }

		friend DenoisePipeline;

		explicit ResourceSet(
			std::shared_ptr<vk::raii::DescriptorPool> pool,
			std::array<std::unique_ptr<vk::raii::DescriptorSet>, FILTER_PASSES> sets,
			vk::Sampler sampler
		) :
			pool(std::move(pool)),
			sets(std::move(sets)),
			sampler(sampler)
		{}

	  public:

		ResourceSet(const ResourceSet&) = delete;
		ResourceSet(ResourceSet&&) = default;
		ResourceSet& operator=(const ResourceSet&) = delete;
		ResourceSet& operator=(ResourceSet&&) = default;
	};
}

#pragma once

#include "common/util/error.hpp"
#include "vulkan/container/device/attachment.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <libassert/assert.hpp>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	///
	/// @brief Attachment for deferred rendering
	/// @details
	/// - Each pixel takes 14 bytes of storage
	/// - See deferred pipeline for detailed layout
	///
	class DeferredAttachment
	{
	  public:

		static constexpr auto ALBEDO_FORMAT = vk::Format::eR8G8B8A8Srgb;  // RGBA8, Srgb, 4 BPP
		static constexpr auto NORMAL_FORMAT = vk::Format::eR16G16Snorm;   // RG16, Snorm, 4 BPP
		static constexpr auto PBR_FORMAT = vk::Format::eR8G8Unorm;        // RG8, Unorm, 2 BPP
		static constexpr auto DEPTH_FORMAT = vk::Format::eD32Sfloat;      // D32, Float, 4 BPP

		///
		/// @brief Create a deferred attachment with given extent
		///
		/// @param context Vulkan context
		/// @param command_buffer Command buffer for clearing images
		/// @param extent Attachment extent
		/// @return Created attachment or error
		///
		[[nodiscard]]
		static std::expected<DeferredAttachment, Error> create(
			const vulkan::Context& context,
			const vk::raii::CommandBuffer& command_buffer,
			glm::u32vec2 extent
		) noexcept;

		///
		/// @brief View of the attachment
		///
		struct View
		{
			glm::u32vec2 extent;
			vulkan::AttachmentView albedo, normal, pbr, depth;

			const View* operator->() const noexcept { return this; }
		};

		operator View() const noexcept
		{
			return {
				.extent = extent,
				.albedo = albedo,
				.normal = normal,
				.pbr = pbr,
				.depth = depth,
			};
		}

		View operator->() const noexcept { return *this; }

	  private:

		glm::u32vec2 extent;
		vulkan::Attachment albedo, normal, pbr, depth;

		explicit DeferredAttachment(
			glm::u32vec2 extent,
			vulkan::Attachment albedo,
			vulkan::Attachment normal,
			vulkan::Attachment pbr,
			vulkan::Attachment depth
		) :
			extent(extent),
			albedo(std::move(albedo)),
			normal(std::move(normal)),
			pbr(std::move(pbr)),
			depth(std::move(depth))
		{}

	  public:

		DeferredAttachment(const DeferredAttachment&) = delete;
		DeferredAttachment(DeferredAttachment&&) = default;
		DeferredAttachment& operator=(const DeferredAttachment&) = delete;
		DeferredAttachment& operator=(DeferredAttachment&&) = default;
	};

	///
	/// @brief Half-resolution deferred attachment
	///
	class HalfDeferredAttachment
	{
	  public:

		static constexpr auto HALF_ALBEDO_STORAGE_FORMAT = vk::Format::eR8G8B8A8Unorm;  // RGBA8, Unorm,
		static constexpr auto HALF_DEPTH_FORMAT = vk::Format::eR32Sfloat;               // R32, Float, 4 BPP
		static constexpr auto HALF_NORMAL_FORMAT = DeferredAttachment::NORMAL_FORMAT;
		static constexpr auto HALF_PBR_FORMAT = DeferredAttachment::PBR_FORMAT;

		///
		/// @brief Create a deferred attachment with given extent
		///
		/// @param context Vulkan context
		/// @param command_buffer Command buffer for clearing images
		/// @param extent Attachment extent
		/// @return Created attachment or error
		///
		[[nodiscard]]
		static std::expected<HalfDeferredAttachment, Error> create(
			const vulkan::Context& context,
			const vk::raii::CommandBuffer& command_buffer,
			glm::u32vec2 extent
		) noexcept;

		///
		/// @brief View of the attachment
		///
		struct View
		{
			glm::u32vec2 full_extent;
			glm::u32vec2 half_extent;
			vulkan::AttachmentView albedo, normal, pbr, depth;

			const View* operator->() const noexcept { return this; }
		};

		operator View() const noexcept
		{
			return {
				.full_extent = full_extent,
				.half_extent = half_extent,
				.albedo = albedo,
				.normal = normal,
				.pbr = pbr,
				.depth = depth,
			};
		}

		View operator->() const noexcept { return *this; }

	  private:

		glm::u32vec2 full_extent;
		glm::u32vec2 half_extent;
		vulkan::Attachment albedo, normal, pbr, depth;

		explicit HalfDeferredAttachment(
			glm::u32vec2 extent,
			glm::u32vec2 half_extent,
			vulkan::Attachment albedo,
			vulkan::Attachment normal,
			vulkan::Attachment pbr,
			vulkan::Attachment depth
		) :
			full_extent(extent),
			half_extent(half_extent),
			albedo(std::move(albedo)),
			normal(std::move(normal)),
			pbr(std::move(pbr)),
			depth(std::move(depth))
		{}

	  public:

		HalfDeferredAttachment(const HalfDeferredAttachment&) = delete;
		HalfDeferredAttachment(HalfDeferredAttachment&&) = default;
		HalfDeferredAttachment& operator=(const HalfDeferredAttachment&) = delete;
		HalfDeferredAttachment& operator=(HalfDeferredAttachment&&) = default;
	};
}

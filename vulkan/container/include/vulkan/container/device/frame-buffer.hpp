#pragma once

#include "common/util/error.hpp"
#include "vulkan/alloc/image.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief Frame buffer helper class, contains an image and its view
	/// @details Designed for creating a frame buffer attachment in a quick fashion
	///
	class FrameBuffer
	{
	  public:

		vk::Format format;
		vulkan::Image image;
		vk::raii::ImageView view;

		operator AttachmentRef() const noexcept
		{
			return {
				.format = format,
				.image = image,
				.view = view,
			};
		}

		///
		/// @brief Create a framebuffer
		///
		/// @param device Vulkan device
		/// @param allocator Vulkan memory allocator
		/// @param extent Extent of the frame buffer
		/// @param format Vulkan format of the frame buffer
		/// @param additional_usage Additional usage flags for the frame buffer image (No need to include
		/// `eXxxAttachment` or `eSampled` bit)
		/// @return Created frame buffer, or error
		///
		[[nodiscard]]
		static std::expected<FrameBuffer, Error> create(
			const vk::raii::Device& device,
			const vulkan::Allocator& allocator,
			glm::u32vec2 extent,
			vk::Format format,
			vk::ImageUsageFlags additional_usage = {}
		) noexcept;

		///
		/// @brief Create a color framebuffer
		///
		/// @param device Vulkan device
		/// @param allocator Vulkan memory allocator
		/// @param extent Extent of the frame buffer
		/// @param format Vulkan format of the frame buffer image
		/// @param additional_usage Additional usage flags for the frame buffer image (No need to include
		/// `ColorAttachment` bit)
		/// @return Created frame buffer, or error
		///
		[[deprecated("Use create() instead")]]
		static std::expected<FrameBuffer, Error> create_color(
			const vk::raii::Device& device,
			const vulkan::Allocator& allocator,
			glm::u32vec2 extent,
			vk::Format format,
			vk::ImageUsageFlags additional_usage = {}
		) noexcept;

		///
		/// @brief Create a depth framebuffer
		///
		/// @param device Vulkan device
		/// @param allocator Vulkan memory allocator
		/// @param extent Extent of the frame buffer
		/// @param format Vulkan format of the frame buffer image
		/// @param additional_usage Additional usage flags for the frame buffer image (No need to include
		/// `DepthStencilAttachment` bit)
		/// @return Created frame buffer, or error
		///
		[[deprecated("Use create() instead")]]
		static std::expected<FrameBuffer, Error> create_depth(
			const vk::raii::Device& device,
			const vulkan::Allocator& allocator,
			glm::u32vec2 extent,
			vk::Format format,
			vk::ImageUsageFlags additional_usage = {}
		) noexcept;

		///
		/// @brief Create a depth stencil framebuffer
		///
		/// @param device Vulkan device
		/// @param allocator Vulkan memory allocator
		/// @param extent Extent of the frame buffer
		/// @param format Vulkan format of the frame buffer image, must be a depth stencil format
		/// @param additional_usage Additional usage flags for the frame buffer image (No need to include
		/// `DepthStencilAttachment` bit)
		/// @return Created frame buffer, or error
		///
		[[deprecated("Use create() instead")]]
		static std::expected<FrameBuffer, Error> create_depth_stencil(
			const vk::raii::Device& device,
			const vulkan::Allocator& allocator,
			glm::u32vec2 extent,
			vk::Format format,
			vk::ImageUsageFlags additional_usage = {}
		) noexcept;

	  private:

		explicit FrameBuffer(vk::Format format, vulkan::Image image, vk::raii::ImageView view) :
			format(format),
			image(std::move(image)),
			view(std::move(view))
		{}

	  public:

		FrameBuffer(const FrameBuffer&) = delete;
		FrameBuffer(FrameBuffer&&) = default;
		FrameBuffer& operator=(const FrameBuffer&) = delete;
		FrameBuffer& operator=(FrameBuffer&&) = default;
	};
}

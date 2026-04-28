#pragma once

#include "common/util/error.hpp"
#include "vulkan/alloc/image.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief Frame buffer helper class, contains an image and its view
	/// @details Designed for creating a frame buffer attachment in a quick fashion
	///
	struct FrameBuffer
	{
		vulkan::Image image;
		vk::raii::ImageView view;

		struct Ref
		{
			vk::Image image;
			vk::ImageView view;
		};

		///
		/// @brief Get references to the image and view
		///
		/// @return References to the image and view
		///
		[[nodiscard]]
		Ref ref() const noexcept
		{
			return Ref{.image = image, .view = view};
		}

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
		static std::expected<FrameBuffer, Error> create_depth_stencil(
			const vk::raii::Device& device,
			const vulkan::Allocator& allocator,
			glm::u32vec2 extent,
			vk::Format format,
			vk::ImageUsageFlags additional_usage = {}
		) noexcept;
	};
}

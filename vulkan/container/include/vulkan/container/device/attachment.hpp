#pragma once

#include "common/util/error.hpp"
#include "vulkan/alloc/image.hpp"
#include "vulkan/interface/attachment.hpp"

#include <cstdint>
#include <expected>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/vector_int4_sized.hpp>
#include <glm/ext/vector_uint2_sized.hpp>
#include <glm/ext/vector_uint4_sized.hpp>
#include <glm/glm.hpp>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief Attachment, contains an image and its view
	/// @details Designed for creating an attachment in a quick fashion
	///
	class Attachment
	{
	  public:

		///
		/// @brief Create a framebuffer
		///
		/// @param device Vulkan device
		/// @param allocator Vulkan memory allocator
		/// @param extent Extent of the frame buffer
		/// @param format Vulkan format of the frame buffer
		/// @param additional_usage Additional usage flags for the frame buffer image (No need to include
		/// `eXxxAttachment` or `eSampled` bit)
		///
		/// @note `eTransferDst` bit is included by default, to support clear functions
		///
		/// @return Created frame buffer, or error
		///
		[[nodiscard]]
		static std::expected<Attachment, Error> create(
			const vk::raii::Device& device,
			const vulkan::Allocator& allocator,
			glm::u32vec2 extent,
			vk::Format format,
			vk::ImageUsageFlags additional_usage = {}
		) noexcept;

		///
		/// @brief Clear color attachment with a specific float-point value
		///
		/// @warning The format must be a float-point or normalized format
		///
		/// @param command_buffer Command buffer
		/// @param color Color to fill the attachment with, defaults to black (0, 0, 0, 0)
		/// @param layout Layout after filling the attachment
		///
		void clear_color_float(
			const vk::raii::CommandBuffer& command_buffer,
			glm::vec4 color = glm::vec4(0.0f),
			vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal
		) const noexcept;

		///
		/// @brief Clear color attachment with a specific unsigned integer value
		///
		/// @warning The format must be an unsigned integer format
		///
		/// @param command_buffer Command buffer
		/// @param color Color to fill the attachment with, defaults to black (0, 0, 0, 0)
		/// @param layout Layout after filling the attachment
		///
		void clear_color_uint(
			const vk::raii::CommandBuffer& command_buffer,
			glm::u32vec4 color = glm::u32vec4(0),
			vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal
		) const noexcept;

		///
		/// @brief Clear color attachment with a specific integer value
		///
		/// @warning The format must be an integer format
		///
		/// @param command_buffer Command buffer
		/// @param color Color to fill the attachment with, defaults to black (0, 0, 0, 0)
		/// @param layout Layout after filling the attachment
		///
		void clear_color_int(
			const vk::raii::CommandBuffer& command_buffer,
			glm::i32vec4 color = glm::i32vec4(0),
			vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal
		) const noexcept;

		///
		/// @brief Clear depth/stsencil attachment with a specific depth/stencil value
		///
		/// @param command_buffer Command buffer
		/// @param depth Depth value to fill the attachment with, defaults to 0
		/// @param stencil Stencil value to fill the attachment with, defaults to 0
		/// @param layout Layout after filling the attachment
		///
		void clear_depth_stencil(
			const vk::raii::CommandBuffer& command_buffer,
			float depth = 0.0,
			uint8_t stencil = 0,
			vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal
		) const noexcept;

		operator AttachmentView() const noexcept
		{
			return {
				.format = format,
				.image = image,
				.view = view,
			};
		}

	  private:

		vk::Format format;
		vulkan::Image image;
		vk::raii::ImageView view;

		explicit Attachment(vk::Format format, vulkan::Image image, vk::raii::ImageView view) :
			format(format),
			image(std::move(image)),
			view(std::move(view))
		{}

	  public:

		Attachment(const Attachment&) = delete;
		Attachment(Attachment&&) = default;
		Attachment& operator=(const Attachment&) = delete;
		Attachment& operator=(Attachment&&) = default;
	};
}

#pragma once

#include "common/util/error.hpp"
#include "vulkan/alloc/image.hpp"
#include "vulkan/interface/attachment.hpp"

#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
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

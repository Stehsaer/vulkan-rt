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

namespace render
{
	///
	/// @brief Generic HDR attachment
	/// @details Each pixel takes 8 bytes of storage
	///
	class HdrAttachment
	{
	  public:

		static constexpr auto HDR_FORMAT = vk::Format::eR16G16B16A16Sfloat;

		///
		/// @brief Create a deferred attachment with given extent
		///
		/// @param context Vulkan context
		/// @param extent Attachment extent
		/// @return Created attachment or error
		///
		[[nodiscard]]
		static std::expected<HdrAttachment, Error> create(
			const vulkan::Context& context,
			glm::u32vec2 extent
		) noexcept;

		///
		/// @brief View of the attachment
		///
		struct View
		{
			glm::u32vec2 extent;
			vulkan::AttachmentView attachment;

			const View* operator->() const noexcept { return this; }
		};

		operator View() const noexcept
		{
			return {
				.extent = extent,
				.attachment = attachment,
			};
		}

		View operator->() const noexcept { return *this; }

	  private:

		glm::u32vec2 extent;
		vulkan::Attachment attachment;

		explicit HdrAttachment(glm::u32vec2 extent, vulkan::Attachment attachment) :
			extent(extent),
			attachment(std::move(attachment))
		{}

	  public:

		HdrAttachment(const HdrAttachment&) = delete;
		HdrAttachment(HdrAttachment&&) = default;
		HdrAttachment& operator=(const HdrAttachment&) = delete;
		HdrAttachment& operator=(HdrAttachment&&) = default;
	};
}

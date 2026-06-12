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
	/// @brief Shadow attachment
	///
	class ShadowAttachment
	{
	  public:

		static constexpr auto SHADOW_FORMAT = vk::Format::eR8Unorm;

		///
		/// @brief Create a shadow attachment
		///
		/// @param context Vulkan context
		/// @param full_extent Full-resolution extent
		/// @return Created attachment or error
		///
		[[nodiscard]]
		static std::expected<ShadowAttachment, Error> create(
			const vulkan::Context& context,
			glm::u32vec2 full_extent
		) noexcept;

		struct View
		{
			glm::u32vec2 half_extent;
			glm::u32vec2 full_extent;
			vulkan::AttachmentView shadow;

			auto operator->() const noexcept { return this; }
		};

		operator View() const noexcept
		{
			return {.half_extent = half_extent, .full_extent = full_extent, .shadow = shadow};
		}

		View operator->() const noexcept { return *this; }

	  private:

		glm::u32vec2 half_extent;
		glm::u32vec2 full_extent;
		vulkan::Attachment shadow;

		explicit ShadowAttachment(
			glm::u32vec2 half_extent,
			glm::u32vec2 full_extent,
			vulkan::Attachment shadow
		) :
			half_extent(half_extent),
			full_extent(full_extent),
			shadow(std::move(shadow))
		{}

	  public:

		ShadowAttachment(const ShadowAttachment&) = delete;
		ShadowAttachment(ShadowAttachment&&) = default;
		ShadowAttachment& operator=(const ShadowAttachment&) = delete;
		ShadowAttachment& operator=(ShadowAttachment&&) = default;
	};
}

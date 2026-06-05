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
	class ForwardAttachments
	{
	  public:

		static constexpr auto DEPTH_FORMAT = vk::Format::eD32Sfloat;
		static constexpr auto HDR_FORMAT = vk::Format::eR16G16B16A16Sfloat;

		[[nodiscard]]
		static std::expected<ForwardAttachments, Error> create(
			const vulkan::Context& context,
			glm::u32vec2 extent
		) noexcept;

		struct View
		{
			glm::u32vec2 extent;
			vulkan::AttachmentView depth, hdr;

			const View* operator->() const noexcept { return this; }
		};

		operator View() const noexcept { return {.extent = extent, .depth = depth, .hdr = hdr}; }
		View operator->() const noexcept { return *this; }

	  private:

		glm::u32vec2 extent;
		vulkan::Attachment depth, hdr;

		explicit ForwardAttachments(glm::u32vec2 extent, vulkan::Attachment depth, vulkan::Attachment hdr) :
			extent(extent),
			depth(std::move(depth)),
			hdr(std::move(hdr))
		{}

	  public:

		ForwardAttachments(const ForwardAttachments&) = delete;
		ForwardAttachments(ForwardAttachments&&) = default;
		ForwardAttachments& operator=(const ForwardAttachments&) = delete;
		ForwardAttachments& operator=(ForwardAttachments&&) = default;
	};
}

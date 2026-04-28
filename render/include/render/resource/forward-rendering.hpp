#pragma once

#include "common/util/error.hpp"
#include "vulkan/interface/common-context.hpp"
#include "vulkan/util/frame-buffer.hpp"

#include <optional>

namespace render
{
	///
	/// @brief Resources produced and internally used by forward rendering pipeline
	///
	class ForwardRenderResource
	{
	  public:

		static constexpr auto DEPTH_FORMAT = vk::Format::eD32Sfloat;
		static constexpr auto HDR_FORMAT = vk::Format::eR16G16B16A16Sfloat;

		ForwardRenderResource() = default;

		///
		/// @brief Check if the resource is complete and ready to use
		/// @note To complete the resource, call @p resize
		///
		/// @return If the resource is ready to use
		///
		[[nodiscard]]
		bool is_complete() const noexcept;

		///
		/// @brief Resize the resource to a given extent
		/// @note After a successful resize, the resource is complete and ready to use
		///
		/// @param context Vulkan context
		/// @param extent New extent
		/// @return `void` if success, or error
		///
		[[nodiscard]]
		std::expected<void, Error> resize(const vulkan::DeviceContext& context, glm::u32vec2 extent) noexcept;

		struct Ref
		{
			// Depth Target
			vulkan::FrameBuffer::Ref depth;

			// HDR Target
			vulkan::FrameBuffer::Ref hdr;

			const Ref* operator->() const noexcept { return this; }
		};

		///
		/// @brief Get references to the buffers
		///
		/// @return References
		///
		[[nodiscard]]
		Ref ref() const noexcept
		{
			assert(is_complete());
			return Ref{
				.depth = depth->ref(),
				.hdr = hdr->ref(),
			};
		}

		Ref operator->() const noexcept { return ref(); }

	  private:

		std::optional<vulkan::FrameBuffer> depth = std::nullopt;
		std::optional<vulkan::FrameBuffer> hdr = std::nullopt;

	  public:

		ForwardRenderResource(const ForwardRenderResource&) = delete;
		ForwardRenderResource(ForwardRenderResource&&) = default;
		ForwardRenderResource& operator=(const ForwardRenderResource&) = delete;
		ForwardRenderResource& operator=(ForwardRenderResource&&) = default;
	};
}

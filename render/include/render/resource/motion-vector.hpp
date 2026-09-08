#pragma once

#include "common/util/error.hpp"
#include "vulkan/container/device/attachment.hpp"
#include "vulkan/interface/attachment.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	class MotionVectorAttachment
	{
	  public:

		static constexpr auto MOTION_VECTOR_FORMAT = vk::Format::eR16G16Sfloat;

		///
		/// @brief Create a motion vector attachment
		///
		/// @param context Vulkan context
		/// @param command_buffer Command buffer for clearing images
		/// @param full_extent Full-resolution extent
		/// @return Created attachment or error
		///
		[[nodiscard]]
		static std::expected<MotionVectorAttachment, Error> create(
			const vulkan::Context& context,
			const vk::raii::CommandBuffer& command_buffer,
			glm::u32vec2 full_extent
		) noexcept;

		struct View
		{
			glm::u32vec2 half_extent;
			glm::u32vec2 full_extent;
			vulkan::AttachmentView motion_vector;

			auto operator->() const noexcept { return this; }
		};

		operator View() const noexcept
		{
			return {
				.half_extent = half_extent,
				.full_extent = full_extent,
				.motion_vector = motion_vector,
			};
		}

		View operator->() const noexcept { return *this; }

	  private:

		glm::u32vec2 half_extent;
		glm::u32vec2 full_extent;
		vulkan::Attachment motion_vector;

		explicit MotionVectorAttachment(
			glm::u32vec2 half_extent,
			glm::u32vec2 full_extent,
			vulkan::Attachment motion_vector
		) :
			half_extent(half_extent),
			full_extent(full_extent),
			motion_vector(std::move(motion_vector))
		{}

	  public:

		MotionVectorAttachment(const MotionVectorAttachment&) = delete;
		MotionVectorAttachment(MotionVectorAttachment&&) = default;
		MotionVectorAttachment& operator=(const MotionVectorAttachment&) = delete;
		MotionVectorAttachment& operator=(MotionVectorAttachment&&) = default;
	};
}

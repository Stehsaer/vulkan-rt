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

		// Format for spatial temporary textures, e.g. current frame's stddev
		static constexpr auto INTERMEDIATE_TEX_FORMAT = vk::Format::eR8Unorm;

		// Format for high-quality history & visibility data
		static constexpr auto VISIBILITY_FORMAT = vk::Format::eR16Unorm;

		// Format for ping-pong buffer containing both mean and variance
		static constexpr auto DENOISE_FORMAT = vk::Format::eR16G16Unorm;

		// Format for bitmask used in upsample
		static constexpr auto UPSAMPLE_BITMASK_FORMAT = vk::Format::eR8Uint;

		///
		/// @brief Create a shadow attachment
		///
		/// @param context Vulkan context
		/// @param command_buffer Command buffer for clearing images
		/// @param full_extent Full-resolution extent
		/// @return Created attachment or error
		///
		[[nodiscard]]
		static std::expected<ShadowAttachment, Error> create(
			const vulkan::Context& context,
			const vk::raii::CommandBuffer& command_buffer,
			glm::u32vec2 full_extent
		) noexcept;

		struct View
		{
			glm::u32vec2 half_extent;
			glm::u32vec2 full_extent;

			vulkan::AttachmentView init_sample;
			vulkan::AttachmentView spatial_mean;
			vulkan::AttachmentView spatial_stddev;
			vulkan::AttachmentView filtered_spatial_stddev;
			vulkan::AttachmentView history;
			vulkan::AttachmentView denoise_alice;
			vulkan::AttachmentView denoise_bob;
			vulkan::AttachmentView upsample_bitmask;
			vulkan::AttachmentView visibility;

			auto operator->() const noexcept { return this; }
		};

		operator View() const noexcept
		{
			return {
				.half_extent = half_extent,
				.full_extent = full_extent,
				.init_sample = init_sample,
				.spatial_mean = spatial_mean,
				.spatial_stddev = spatial_stddev,
				.filtered_spatial_stddev = filtered_spatial_stddev,
				.history = history,
				.denoise_alice = denoise_alice,
				.denoise_bob = denoise_bob,
				.upsample_bitmask = upsample_bitmask,
				.visibility = visibility,
			};
		}

		View operator->() const noexcept { return *this; }

	  private:

		glm::u32vec2 half_extent;
		glm::u32vec2 full_extent;

		vulkan::Attachment init_sample;              // Initial sampling
		vulkan::Attachment spatial_mean;             // Spatial mean
		vulkan::Attachment spatial_stddev;           // Spatial stddev
		vulkan::Attachment filtered_spatial_stddev;  // Filtered/Dilated spatial stddev
		vulkan::Attachment history;                  // History visibility
		vulkan::Attachment denoise_alice;            // Ping-pong buffer A
		vulkan::Attachment denoise_bob;              // Ping-pong buffer B
		vulkan::Attachment upsample_bitmask;         // Upsample bitmask
		vulkan::Attachment visibility;               // High-resolution visibility result

		explicit ShadowAttachment(
			glm::u32vec2 half_extent,
			glm::u32vec2 full_extent,
			vulkan::Attachment init_sample,
			vulkan::Attachment spatial_mean,
			vulkan::Attachment spatial_stddev,
			vulkan::Attachment filtered_spatial_stddev,
			vulkan::Attachment history,
			vulkan::Attachment denoise_alice,
			vulkan::Attachment denoise_bob,
			vulkan::Attachment upsample_bitmask,
			vulkan::Attachment visibility
		) :
			half_extent(half_extent),
			full_extent(full_extent),
			init_sample(std::move(init_sample)),
			spatial_mean(std::move(spatial_mean)),
			spatial_stddev(std::move(spatial_stddev)),
			filtered_spatial_stddev(std::move(filtered_spatial_stddev)),
			history(std::move(history)),
			denoise_alice(std::move(denoise_alice)),
			denoise_bob(std::move(denoise_bob)),
			upsample_bitmask(std::move(upsample_bitmask)),
			visibility(std::move(visibility))
		{}

	  public:

		ShadowAttachment(const ShadowAttachment&) = delete;
		ShadowAttachment(ShadowAttachment&&) = default;
		ShadowAttachment& operator=(const ShadowAttachment&) = delete;
		ShadowAttachment& operator=(ShadowAttachment&&) = default;
	};
}

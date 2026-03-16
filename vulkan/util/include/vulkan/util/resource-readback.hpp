#pragma once

#include "common/util/error.hpp"
#include "image/image.hpp"
#include "vulkan/interface/common-context.hpp"

#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	namespace impl
	{
		std::expected<void, Error> readback_image(
			const vulkan::DeviceContext& context,
			vk::Image src_image,
			vk::ImageLayout src_image_layout,
			vk::Format target_format,
			glm::u32vec2 size,
			std::span<std::byte> output_data
		) noexcept;
	}

	///
	/// @brief Read back an image from GPU to CPU, designed to use in testing and verification
	/// @warning Do not use it in frame-loops, this is not designed to be efficient
	/// @note Make sure the target GPU image to readback from is created with
	/// `vk::ImageUsageFlagBits::eTransferSrc`. The validation layer will warn if not.
	///
	/// @tparam T Format of the resulting image
	/// @tparam L Layout of the resulting image
	/// @param device Vulkan device
	/// @param allocator Vulkan memory allocator
	/// @param queue Vulkan queue for submitting commands
	/// @param queue_family Queue family index of the queue
	/// @param src_image Source image on GPU
	/// @param src_image_layout Current layout of the source image
	/// @param target_format Format to read back as (must be compatible with the CPU image format)
	/// @param size Size of the image to read back
	/// @return Read back image or error
	///
	template <image::Format T, image::Layout L>
	std::expected<image::Image<T, L>, Error> readback_image(
		const vulkan::DeviceContext& context,
		vk::Image src_image,
		vk::ImageLayout src_image_layout,
		vk::Format target_format,
		glm::u32vec2 size
	) noexcept
	{
		auto readback_image = image::Image<T, L>(size);

		const auto readback_result = impl::readback_image(
			context,
			src_image,
			src_image_layout,
			target_format,
			size,
			util::as_writable_bytes(readback_image.data)
		);
		if (!readback_result) return readback_result.error();
		return readback_image;
	}
}

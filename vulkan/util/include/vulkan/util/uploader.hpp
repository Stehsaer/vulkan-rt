#pragma once

#include "common/util/error.hpp"
#include "vulkan/alloc.hpp"

#include <vector>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief Simple uploader for buffer and image data using staging buffers.
	/// @note The implementation isn't the most optimized, but good enough for initializing resources.
	/// @details
	/// #### Add upload task
	/// Add upload tasks by filling in `BufferUploadParam` and `ImageUploadParam` and calling `upload_buffer`
	/// or `upload_image`
	///
	/// #### Execute upload tasks
	/// Call `execute` to execute upload tasks. After a successful call of `execute`, images are already in
	/// layouts designated by `ImageUploadParam`
	///
	class Uploader
	{
	  public:

		Uploader(
			const vk::raii::Device& device,
			const vk::raii::Queue& transfer_queue,
			uint32_t queue_family,
			const vulkan::alloc::Allocator& allocator
		) :
			device(device),
			transfer_queue(transfer_queue),
			queue_family(queue_family),
			allocator(allocator)
		{}

		struct BufferUploadParam
		{
			vk::Buffer dst_buffer;            // Destination buffer for uploading
			std::span<const std::byte> data;  // Data to upload
		};

		struct ImageUploadParam
		{
			vk::Image dst_image;                            // Destination image for uploading
			std::span<const std::byte> data;                // Data to upload
			size_t buffer_row_length;                       // In texels
			size_t buffer_image_height;                     // In texels
			vk::ImageSubresourceLayers subresource_layers;  // Subresource layers to upload to
			vk::Extent3D image_extent;                      // Extent of the image to upload
			vk::ImageLayout dst_layout;                     // Final layout of the image after upload
		};

		///
		/// @brief Add buffer upload task
		/// @note
		/// - Actual upload is deferred and executed in `execute()`
		/// - The data is already copied into staging buffer by this call.
		///
		/// @param param Upload parameters, see `BufferUploadParam`
		/// @return `void` on success, `Error` on failure
		///
		[[nodiscard]]
		std::expected<void, Error> upload_buffer(const BufferUploadParam& param) noexcept;

		///
		/// @brief Add image upload task
		/// @note
		/// - Actual upload is deferred and executed in `execute()`
		/// - The data is already copied into staging buffer by this call.
		///
		/// @param param Upload parameters, see `ImageUploadParam`
		/// @return `void` on success, `Error` on failure
		///
		[[nodiscard]]
		std::expected<void, Error> upload_image(const ImageUploadParam& param) noexcept;

		///
		/// @brief Execute the actual uploading of all added tasks.
		/// @note This call blocks until all uploads complete or fail.
		///
		/// @return `void` on success, `Error` on failure
		///
		[[nodiscard]]
		std::expected<void, Error> execute() noexcept;

	  private:

		const vk::raii::Device& device;
		const vk::raii::Queue& transfer_queue;
		const uint32_t queue_family;
		const vulkan::alloc::Allocator& allocator;

		struct BufferUploadTask
		{
			vk::Buffer dst_buffer;

			vulkan::alloc::Buffer staging_buffer;
			size_t data_size;
		};

		struct ImageUploadTask
		{
			vk::Image dst_image;

			vulkan::alloc::Buffer staging_buffer;

			size_t buffer_row_length;
			size_t buffer_image_height;

			vk::ImageSubresourceLayers subresource_layers;
			vk::Extent3D image_extent;

			vk::ImageLayout dst_layout;
		};

		std::vector<BufferUploadTask> buffer_upload_tasks;
		std::vector<ImageUploadTask> image_upload_tasks;
	};
}

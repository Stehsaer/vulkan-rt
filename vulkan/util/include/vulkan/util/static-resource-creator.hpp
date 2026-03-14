#pragma once

#include "common/util/error.hpp"
#include "common/util/span.hpp"
#include "image/bc-image.hpp"
#include "image/image.hpp"
#include "vulkan/alloc.hpp"
#include "vulkan/util/constants.hpp"

#include <functional>
#include <mutex>
#include <ranges>
#include <span>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace vulkan
{
	///
	/// @brief Static resource creator, designed for creating buffers and images at initialization/loading
	/// stage
	///
	/// @warning
	/// - Do not create multiple upload tasks for the same image subresource layer
	/// - Make sure to call `execute_uploads()` after creating resources and before deconstructing the
	/// creator
	/// - Do not use it in frame loops, as this is not designed to be highly efficient. Manually upload in
	/// such scenarios.
	///
	/// @note All resosurces are created as `GpuOnly`
	///
	class StaticResourceCreator
	{
	  public:

		///
		/// @brief Construct a new `StaticResourceCreator`
		///
		/// @param device Vulkan device
		/// @param allocator Vulkan memory allocator
		/// @param transfer_queue Vulkan queue for submitting transfer commands
		/// @param queue_family Queue family index of the transfer queue
		///
		explicit StaticResourceCreator(
			const vk::raii::Device& device,
			const alloc::Allocator& allocator,
			const vk::raii::Queue& transfer_queue,
			uint32_t queue_family
		) noexcept :
			device(device),
			allocator(allocator),
			transfer_queue(transfer_queue),
			queue_family(queue_family),
			execution_mutex(std::make_unique<std::mutex>())
		{}

		~StaticResourceCreator() noexcept;

		///
		/// @brief Create a buffer
		/// @note This function is multi-threading safe
		///
		/// @param data Data to upload to the buffer
		/// @param usage Buffer usage flags (No need to include `TransferDst` bit)
		/// @return Created buffer, or error
		///
		[[nodiscard]]
		std::expected<alloc::Buffer, Error> create_buffer(
			std::span<const std::byte> data,
			vk::BufferUsageFlags usage
		) noexcept;

		///
		/// @brief Create a image from CPU-side image
		/// @note This function is multi-threading safe
		///
		/// @tparam T Image format
		/// @tparam L Image layout
		/// @param image CPU-side image to create the GPU image from
		/// @param format Vulkan format of the created image, must be compatible with the CPU-side image
		/// format
		/// @param usage Vulkan image usage flags (No need to include `TransferDst` bit)
		/// @param layout Vulkan image layout to transition the created image to after upload
		/// @return Created image, or error
		///
		template <image::Format T, image::Layout L>
		[[nodiscard]]
		std::expected<alloc::Image, Error> create_image(
			const image::Image<T, L>& image,
			vk::Format format,
			vk::ImageUsageFlags usage,
			vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal
		) noexcept;

		///
		/// @brief Create a image from CPU-side BCn image
		/// @note This function is multi-threading safe
		///
		/// @param image CPU-side BCn image to create the GPU image from
		/// @param srgb Whether the image is in sRGB color space
		/// @param usage Vulkan image usage flags (No need to include `TransferDst` bit)
		/// @param layout Vulkan image layout to transition the created image to after upload
		/// @return Created image, or error
		///
		[[nodiscard]]
		std::expected<alloc::Image, Error> create_image_bcn(
			const image::BCnImage& image,
			bool srgb,
			vk::ImageUsageFlags usage,
			vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal
		) noexcept;

		///
		/// @brief Create a image with multiple mipmap levels from CPU-side images chain
		/// @note This function is multi-threading safe
		///
		/// @tparam T Image format
		/// @tparam L Image layout
		/// @param mipmap_chain CPU-side images for each mipmap level, ordered from largest to smallest
		/// @param format Vulkan format of the created image, must be compatible with the CPU-side image
		/// format
		/// @param usage Vulkan image usage flags (No need to include `TransferDst` bit)
		/// @param layout Vulkan image layout to transition the created image to after upload
		/// @return Created image, or error
		///
		template <image::Format T, image::Layout L>
		[[nodiscard]]
		std::expected<alloc::Image, Error> create_image_mipmap(
			std::span<const image::Image<T, L>> mipmap_chain,
			vk::Format format,
			vk::ImageUsageFlags usage,
			vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal
		) noexcept;

		///
		/// @brief Create a image with multiple mipmap levels from CPU-side images chain
		/// @note This function is multi-threading safe
		///
		/// @param mipmap_chain CPU-side images for each mipmap level, ordered from largest to smallest
		/// @param format Vulkan format of the created image, must be compatible with the CPU-side image
		/// format
		/// @param usage Vulkan image usage flags (No need to include `TransferDst` bit)
		/// @param layout Vulkan image layout to transition the created image to after upload
		/// @return Created image, or error
		///
		template <std::ranges::input_range Range>
			requires(std::ranges::contiguous_range<Range>)
		std::expected<alloc::Image, Error> create_image_mipmap(
			Range&& mipmap_chain,
			vk::Format format,
			vk::ImageUsageFlags usage,
			vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal
		) noexcept
		{
			using RangeValueType = std::add_const_t<std::ranges::range_value_t<Range>>;
			return create_image_mipmap(std::span<RangeValueType>(mipmap_chain), format, usage, layout);
		}

		///
		/// @brief Create a image with multiple mipmap levels from CPU-side BCn images chain
		/// @note This function is multi-threading safe
		///
		/// @param mipmap_chain CPU-side images for each mipmap level, ordered from largest to smallest
		/// @param format Vulkan format of the created image, must be compatible with the CPU-side image
		/// format
		/// @param usage Vulkan image usage flags (No need to include `TransferDst` bit)
		/// @param layout Vulkan image layout to transition the created image to after upload
		/// @return Created image, or error
		///
		[[nodiscard]]
		std::expected<alloc::Image, Error> create_image_mipmap_bcn(
			const std::span<const image::BCnImage>& mipmap_chain,
			bool srgb,
			vk::ImageUsageFlags usage,
			vk::ImageLayout layout = vk::ImageLayout::eShaderReadOnlyOptimal
		) noexcept;

		///
		/// @brief Execute all upload tasks
		/// @note
		/// - No matter success or fail, tasks will be cleared after this call
		/// - This function is multi-threading safe
		///
		/// @return `void` if all uploads succeed, or `Error` describing reason of failure if any upload fails
		///
		[[nodiscard]]
		std::expected<void, Error> execute_uploads() noexcept;

		///
		/// @brief Get number of pending upload tasks
		/// @note This function is multi-threading safe
		///
		/// @return
		///
		[[nodiscard]]
		size_t num_pending() const noexcept;

		///
		/// @brief Get total size of pending upload data in bytes
		/// @note This function is multi-threading safe
		///
		/// @return Total size of pending upload data in bytes
		///
		[[nodiscard]]
		size_t size_pending() const noexcept
		{
			return pending_data_size;
		}

		///
		/// @brief Check if there are any pending upload tasks
		/// @note This function is multi-threading safe
		///
		/// @return `true` if there are pending upload tasks, `false` otherwise
		///
		[[nodiscard]]
		bool has_pending() const noexcept
		{
			return num_pending() > 0;
		}

	  private:

		struct BufferUploadTask
		{
			vk::Buffer dst_buffer;
			vulkan::alloc::Buffer staging_buffer;
			size_t data_size;
		};

		struct ImageUploadTask
		{
			vk::Image dst_image;
			alloc::Buffer staging_buffer;
			vk::ImageSubresourceLayers subresource_layers;
			vk::Extent3D image_extent;
			vk::ImageLayout dst_layout;

			[[nodiscard]]
			vk::ImageMemoryBarrier2 get_barrier_pre() const;

			[[nodiscard]]
			vk::ImageMemoryBarrier2 get_barrier_post() const;
		};

		std::reference_wrapper<const vk::raii::Device> device;
		std::reference_wrapper<const vulkan::alloc::Allocator> allocator;
		std::reference_wrapper<const vk::raii::Queue> transfer_queue;
		uint32_t queue_family;
		std::unique_ptr<std::mutex> execution_mutex;

		std::vector<BufferUploadTask> buffer_upload_tasks;
		std::vector<ImageUploadTask> image_upload_tasks;
		size_t pending_data_size = 0;

		///
		/// @brief Create a staging buffer and upload data to it.
		///
		/// @param data Data to upload to the staging buffer
		/// @return Created staging buffer, or error
		///
		[[nodiscard]]
		std::expected<alloc::Buffer, Error> create_staging_buffer(std::span<const std::byte> data) noexcept;

		///
		/// @brief Check mipmap chain sizes for validity.
		///
		/// @param sizes Sizes of the mipmap levels, in order from largest to smallest
		/// @return `void` if the sizes are valid, or `Error` describing reason of being invalid
		///
		[[nodiscard]]
		static std::expected<void, Error> check_mipmap_chain_sizes(std::vector<glm::u32vec2> sizes) noexcept;

	  public:

		StaticResourceCreator(const StaticResourceCreator&) = delete;
		StaticResourceCreator(StaticResourceCreator&&) = default;
		StaticResourceCreator& operator=(const StaticResourceCreator&) = delete;
		StaticResourceCreator& operator=(StaticResourceCreator&&) = default;
	};

	/* Implementations */

	template <image::Format T, image::Layout L>
	std::expected<alloc::Image, Error> StaticResourceCreator::create_image(
		const image::Image<T, L>& image,
		vk::Format format,
		vk::ImageUsageFlags usage,
		vk::ImageLayout layout
	) noexcept
	{

		const auto extent = vk::Extent3D{.width = image.size.x, .height = image.size.y, .depth = 1};
		const auto subresource_layer = vulkan::base_level_image_layer(vk::ImageAspectFlagBits::eColor);

		const auto image_create_info = vk::ImageCreateInfo{
			.imageType = vk::ImageType::e2D,
			.format = format,
			.extent = extent,
			.mipLevels = 1,
			.arrayLayers = 1,
			.usage = usage | vk::ImageUsageFlagBits::eTransferDst
		};
		auto image_result =
			allocator.get().create_image(image_create_info, vulkan::alloc::MemoryUsage::GpuOnly);
		if (!image_result) return image_result.error().forward("Create gpu image failed");
		auto dst_image = std::move(*image_result);

		auto staging_buffer_result = create_staging_buffer(util::as_bytes(image.data));
		if (!staging_buffer_result)
			return staging_buffer_result.error().forward("Create staging buffer failed");

		const std::scoped_lock lock(*execution_mutex);
		pending_data_size += image.data.size_bytes();
		image_upload_tasks.push_back(
			ImageUploadTask{
				.dst_image = dst_image,
				.staging_buffer = std::move(*staging_buffer_result),
				.subresource_layers = subresource_layer,
				.image_extent = extent,
				.dst_layout = layout
			}
		);

		return dst_image;
	}

	template <image::Format T, image::Layout L>
	std::expected<alloc::Image, Error> StaticResourceCreator::create_image_mipmap(
		std::span<const image::Image<T, L>> mipmap_chain,
		vk::Format format,
		vk::ImageUsageFlags usage,
		vk::ImageLayout layout
	) noexcept
	{

		/* Verify inputs */

		// Empty mipmap chain
		if (mipmap_chain.empty()) return Error("Input mipmap chain is empty");

		// Verify sizes
		if (const auto size_check_result = check_mipmap_chain_sizes(
				mipmap_chain
				| std::views::transform([](const auto& image) { return image.size; })
				| std::ranges::to<std::vector>()
			);
			!size_check_result)
		{
			return size_check_result.error();
		}

		/* Create image */

		const std::vector<vk::Extent3D> extents =
			mipmap_chain
			| std::views::transform([](const auto& image) {
				  return vk::Extent3D{.width = image.size.x, .height = image.size.y, .depth = 1};
			  })
			| std::ranges::to<std::vector>();
		const uint32_t mipmap_levels = mipmap_chain.size();

		const auto image_create_info = vk::ImageCreateInfo{
			.imageType = vk::ImageType::e2D,
			.format = format,
			.extent = extents[0],
			.mipLevels = mipmap_levels,
			.arrayLayers = 1,
			.usage = usage | vk::ImageUsageFlagBits::eTransferDst
		};
		auto image_result =
			allocator.get().create_image(image_create_info, vulkan::alloc::MemoryUsage::GpuOnly);
		if (!image_result) return image_result.error().forward("Create gpu image failed");
		auto dst_image = std::move(*image_result);

		/* Create staging buffers */

		auto staging_buffer_result =
			mipmap_chain
			| std::views::transform([this](const auto& image) {
				  return create_staging_buffer(util::as_bytes(image.data));
			  })
			| Error::collect();
		if (!staging_buffer_result)
			return staging_buffer_result.error().forward("Create staging buffers failed");
		std::vector<alloc::Buffer> staging_buffers = std::move(*staging_buffer_result);

		/* Append task */

		const std::vector<vk::ImageSubresourceLayers> subresource_layers =
			std::views::iota(0u, mipmap_levels)
			| std::views::transform([](uint32_t mip_level) {
				  return vk::ImageSubresourceLayers{
					  .aspectMask = vk::ImageAspectFlagBits::eColor,
					  .mipLevel = mip_level,
					  .baseArrayLayer = 0,
					  .layerCount = 1,
				  };
			  })
			| std::ranges::to<std::vector>();

		const auto as_upload_task =
			[&dst_image, layout](const auto& extent, const auto& subresource_layer, auto& staging_buffer) {
				return ImageUploadTask{
					.dst_image = dst_image,
					.staging_buffer = std::move(staging_buffer),
					.subresource_layers = subresource_layer,
					.image_extent = extent,
					.dst_layout = layout
				};
			};

		const std::scoped_lock lock(*execution_mutex);
		pending_data_size +=
			std::ranges::fold_left_first(
				mipmap_chain | std::views::transform([](const auto& image) {
					return std::span(image.data).size_bytes();
				}),
				std::plus()
			)
				.value_or(0);
		image_upload_tasks.append_range(
			std::views::zip_transform(as_upload_task, extents, subresource_layers, staging_buffers)
		);

		return dst_image;
	}
}

#pragma once

#include "common/util/error.hpp"
#include <expected>
#include <memory>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace vulkan::alloc
{
	enum class MemoryUsage
	{
		Unknown = VMA_MEMORY_USAGE_UNKNOWN,
		GpuOnly = VMA_MEMORY_USAGE_GPU_ONLY,
		CpuOnly = VMA_MEMORY_USAGE_CPU_ONLY,
		CpuToGpu = VMA_MEMORY_USAGE_CPU_TO_GPU,
		GpuToCpu = VMA_MEMORY_USAGE_GPU_TO_CPU,
		CpuCopy = VMA_MEMORY_USAGE_CPU_COPY,
		GpuLazilyAllocated = VMA_MEMORY_USAGE_GPU_LAZILY_ALLOCATED,
		Auto = VMA_MEMORY_USAGE_AUTO,
		AutoPreferDevice = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
		AutoPreferHost = VMA_MEMORY_USAGE_AUTO_PREFER_HOST
	};

	struct AllocatorWrapper
	{
		VmaAllocator allocator;

		AllocatorWrapper(VmaAllocator allocator) :
			allocator(allocator)
		{}

		~AllocatorWrapper() noexcept;
	};

	struct ImageWrapper
	{
		vk::Image image;
		VmaAllocation allocation;
		VmaAllocator allocator;

		ImageWrapper(vk::Image image, VmaAllocation allocation, VmaAllocator allocator) :
			image(image),
			allocation(allocation),
			allocator(allocator)
		{}

		~ImageWrapper() noexcept;
	};

	struct BufferWrapper
	{
		vk::Buffer buffer;
		VmaAllocation allocation;
		VmaAllocator allocator;

		BufferWrapper(vk::Buffer buffer, VmaAllocation allocation, VmaAllocator allocator) :
			buffer(buffer),
			allocation(allocation),
			allocator(allocator)
		{}

		~BufferWrapper() noexcept;
	};

	class Image
	{
		std::unique_ptr<ImageWrapper> wrapper;

		Image(std::unique_ptr<ImageWrapper> wrapper) :
			wrapper(std::move(wrapper))
		{}

		friend class Allocator;

	  public:

		Image(const Image&) = delete;
		Image(Image&&) = default;
		Image& operator=(const Image&) = delete;
		Image& operator=(Image&&) = default;

		operator vk::Image() noexcept { return wrapper->image; }
	};

	class Buffer
	{
		std::unique_ptr<BufferWrapper> wrapper;

		Buffer(std::unique_ptr<BufferWrapper> wrapper) :
			wrapper(std::move(wrapper))
		{}

		friend class Allocator;

	  public:

		Buffer(const Buffer&) = delete;
		Buffer(Buffer&&) = default;
		Buffer& operator=(const Buffer&) = delete;
		Buffer& operator=(Buffer&&) = default;

		operator vk::Buffer() const noexcept { return wrapper->buffer; }

		///
		/// @brief Upload data to buffer
		///
		/// @param data Host data
		/// @param dst_offset Destination offset in buffer
		/// @return Void or Error
		///
		std::expected<void, Error> upload(
			std::span<const std::byte> data,
			size_t dst_offset = 0
		) const noexcept;

		///
		/// @brief Download data from buffer to host
		///
		/// @param data Data span to fill
		/// @param src_offset Source offset in buffer
		/// @return Void or Error
		///
		std::expected<void, Error> download(std::span<std::byte> data, size_t src_offset = 0) const noexcept;
	};

	class Allocator
	{
	  public:

		///
		/// @brief Create an allocator
		///
		/// @param instance Vulkan Instance
		/// @param physical_device Vulkan Physical Device
		/// @param device Vulkan Device
		/// @return An allocator or Error
		///
		static std::expected<Allocator, Error> create(
			const vk::Instance& instance,
			const vk::PhysicalDevice& physical_device,
			const vk::Device& device
		) noexcept;

		///
		/// @brief Create an image
		///
		/// @param create_info Vulkan create info
		/// @param usage VMA memory usage
		/// @return An image or Error
		///
		std::expected<Image, Error> create_image(
			const vk::ImageCreateInfo& create_info,
			MemoryUsage usage
		) const noexcept;

		///
		/// @brief Create a buffer
		///
		/// @param create_info Vulkan create info
		/// @param usage VMA memory usage
		/// @return A buffer or Error
		///
		std::expected<Buffer, Error> create_buffer(
			const vk::BufferCreateInfo& create_info,
			MemoryUsage usage
		) const noexcept;

	  private:

		std::unique_ptr<AllocatorWrapper> wrapper;

		Allocator(std::unique_ptr<AllocatorWrapper> wrapper) :
			wrapper(std::move(wrapper))
		{}

	  public:

		Allocator(const Allocator&) = delete;
		Allocator(Allocator&&) = default;
		Allocator& operator=(const Allocator&) = delete;
		Allocator& operator=(Allocator&&) = default;
	};
}
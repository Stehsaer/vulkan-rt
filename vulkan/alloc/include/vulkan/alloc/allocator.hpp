#pragma once

#include "common/util/error.hpp"
#include "vulkan/alloc/buffer.hpp"
#include "vulkan/alloc/image.hpp"

#include <expected>
#include <memory>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace vulkan
{
	///
	/// @brief Memory usage for allocation
	///
	enum class MemoryUsage
	{
		GpuOnly,
		CpuToGpu,
		GpuToCpu,
	};

	///
	/// @brief C++ wrapper for vulkan-memory-allocator
	/// @details
	/// #### Creation
	/// Simply use `vulkan::Allocator::create` to create an allocator instance.
	///
	/// #### Allocation
	/// Use `allocator.create_image(...)` and `allocator.create_buffer(...)` to create buffers and images.
	/// These buffers and images are self-contained wrappers and automatically frees the memory on
	/// deconstruction. For buffers, there are also type-safe versions `ElementBuffer` and `ArrayBuffer` that
	/// hold elements of type `T` and `T[]` respectively.
	///
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
		[[nodiscard]]
		static std::expected<Allocator, Error> create(
			const vk::raii::Instance& instance,
			const vk::raii::PhysicalDevice& physical_device,
			const vk::raii::Device& device
		) noexcept;

		///
		/// @brief Create an image
		///
		/// @param create_info Vulkan create info
		/// @param usage VMA memory usage
		/// @return An image or Error
		///
		[[nodiscard]]
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
		[[nodiscard]]
		std::expected<Buffer, Error> create_buffer(
			const vk::BufferCreateInfo& create_info,
			MemoryUsage usage
		) const noexcept;

		///
		/// @brief Create an element buffer that holds a single element of type T
		///
		/// @tparam T Type of the element
		/// @param usage_flags Vulkan buffer usage flags
		/// @param usage VMA memory usage
		/// @param sharing_mode Vulkan buffer sharing mode
		/// @param queue_family_indices Queue family indices for concurrent sharing mode
		/// @return An element buffer or Error
		///
		template <typename T>
		[[nodiscard]]
		std::expected<ElementBuffer<T>, Error> create_element_buffer(
			vk::BufferUsageFlags usage_flags,
			MemoryUsage usage,
			vk::SharingMode sharing_mode = vk::SharingMode::eExclusive,
			std::span<const uint32_t> queue_family_indices = {}
		) const noexcept
		{
			auto buffer_result = create_buffer(
				{
					.size = sizeof(T),
					.usage = usage_flags,
					.sharingMode = sharing_mode,
					.queueFamilyIndexCount = uint32_t(queue_family_indices.size()),
					.pQueueFamilyIndices = queue_family_indices.data(),
				},
				usage
			);
			if (!buffer_result) return buffer_result.error();
			return ElementBuffer<T>(std::move(*buffer_result));
		}

		///
		/// @brief Create an array buffer that holds an array of elements of type T
		///
		/// @tparam T Type of the elements
		/// @param element_count Number of elements the buffer can hold
		/// @param usage_flags Vulkan buffer usage flags
		/// @param usage VMA memory usage
		/// @param sharing_mode Vulkan buffer sharing mode
		/// @param queue_family_indices Queue family indices for concurrent sharing mode
		/// @return An array buffer or Error
		///
		template <typename T>
		[[nodiscard]]
		std::expected<ArrayBuffer<T>, Error> create_array_buffer(
			size_t element_count,
			vk::BufferUsageFlags usage_flags,
			MemoryUsage usage,
			vk::SharingMode sharing_mode = vk::SharingMode::eExclusive,
			std::span<const uint32_t> queue_family_indices = {}
		) const noexcept
		{
			auto buffer_result = create_buffer(
				{
					.size = element_count * sizeof(T),
					.usage = usage_flags,
					.sharingMode = sharing_mode,
					.queueFamilyIndexCount = uint32_t(queue_family_indices.size()),
					.pQueueFamilyIndices = queue_family_indices.data(),
				},
				usage
			);
			if (!buffer_result) return buffer_result.error();
			return ArrayBuffer<T>(std::move(*buffer_result), element_count);
		}

	  private:

		std::unique_ptr<impl::AllocatorWrapper> wrapper;

		Allocator(std::unique_ptr<impl::AllocatorWrapper> wrapper) :
			wrapper(std::move(wrapper))
		{}

	  public:

		Allocator(const Allocator&) = delete;
		Allocator(Allocator&&) = default;
		Allocator& operator=(const Allocator&) = delete;
		Allocator& operator=(Allocator&&) = default;
	};
}

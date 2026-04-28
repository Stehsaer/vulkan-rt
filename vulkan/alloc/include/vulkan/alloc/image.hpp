#pragma once

#include "vulkan/alloc/wrapper.hpp"

#include <memory>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace vulkan
{
	class Allocator;

	///
	/// @brief Allocated image, frees the memory on deconstruction
	/// @details Can be directly used as if it were a `vk::Image`
	///
	class Image
	{
	  public:

		operator vk::Image() const noexcept { return wrapper->image; }
		vk::Image operator*() const noexcept { return wrapper->image; }

	  private:

		std::unique_ptr<impl::ImageWrapper> wrapper;

		explicit Image(std::unique_ptr<impl::ImageWrapper> wrapper) :
			wrapper(std::move(wrapper))
		{}

		friend class ::vulkan::Allocator;

	  public:

		Image(const Image&) = delete;
		Image(Image&&) = default;
		Image& operator=(const Image&) = delete;
		Image& operator=(Image&&) = default;
	};
}

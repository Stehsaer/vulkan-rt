#pragma once

#include <cstdint>
#include <expected>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "common/util/error.hpp"
#include "vulkan/context-impl/vulkan.hpp"

namespace vulkan
{
	// Struct holding a swapchain image and its view
	struct SwapchainImage
	{
		vk::Image image;
		vk::raii::ImageView view;
	};

	// Struct holding the result of acquiring a swapchain image
	struct SwapchainAcquireResult
	{
		glm::u32vec2 extent;
		uint32_t image_index;
		const SwapchainImage& image;
	};

	///
	/// @brief Class managing a Vulkan swapchain and its images, may be recreated as needed
	///
	///
	class SwapchainInstance
	{
	  public:

		///
		/// @brief Create swapchain for the given window and vulkan context
		///
		/// @param window Window context
		/// @param vulkan Vulkan context
		/// @param old_swapchain Optional old swapchain for recreation
		/// @return New Swapchain instance or Error
		///
		static std::expected<SwapchainInstance, Error> create(
			const vk::raii::PhysicalDevice& phy_device,
			const vk::raii::Device& device,
			vk::SurfaceKHR surface,
			const SwapchainLayout& swapchain_layout,
			std::optional<SwapchainInstance> old_swapchain
		) noexcept;

		///
		/// @brief Get the number of images in the swapchain
		///
		/// @return Number of images
		///
		size_t count() const noexcept { return images.size(); }

		///
		/// @brief Get the extent (width and height) of the swapchain images
		///
		/// @return Extent as `glm::u32vec2`
		///
		glm::u32vec2 size() const noexcept { return extent; }

		///
		/// @brief Acquire the next image from the swapchain
		///
		/// @param semaphore Semaphore object, can be empty
		/// @param fence Fence object, can be empty
		/// @param timeout Timeout, default to UINT64_MAX
		/// @return Acquired image or Vulkan warning/error
		///
		std::expected<SwapchainAcquireResult, vk::Result> acquire_next_image(
			std::optional<vk::Semaphore> semaphore = std::nullopt,
			std::optional<vk::Fence> fence = std::nullopt,
			uint64_t timeout = std::numeric_limits<uint64_t>::max()
		) noexcept;

		///
		/// @brief Get the current swapchain image
		///
		/// @param index Index of the image to get
		/// @return Reference to the swapchain image
		///
		const SwapchainImage& operator[](size_t index) const noexcept { return images.at(index); }

		vk::SwapchainKHR get_swapchain() const noexcept { return swapchain; }

		SwapchainInstance(const SwapchainInstance&) = delete;
		SwapchainInstance(SwapchainInstance&&) noexcept = default;
		SwapchainInstance& operator=(const SwapchainInstance&) = delete;
		SwapchainInstance& operator=(SwapchainInstance&&) = default;

	  private:

		std::vector<SwapchainImage> images;
		vk::raii::SwapchainKHR swapchain;
		glm::u32vec2 extent;

		SwapchainInstance(
			std::vector<SwapchainImage> images,
			vk::raii::SwapchainKHR swapchain,
			glm::u32vec2 extent
		) :
			images(std::move(images)),
			swapchain(std::move(swapchain)),
			extent(extent)
		{}
	};
}
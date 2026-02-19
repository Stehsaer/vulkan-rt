#pragma once

#include <glm/glm.hpp>
#include <optional>
#include <variant>
#include <vulkan/vulkan_raii.hpp>

#include "common/util/error.hpp"
#include "vulkan/context/device.hpp"
#include "vulkan/context/instance.hpp"

namespace vulkan
{
	///
	/// @brief Manages swapchain context.
	/// @details
	/// #### Creation
	/// Call @p create to create a swapchain context. Make sure to create an `InstanceContext` and
	/// `DeviceContext` prior to creating this context.
	///
	/// #### Swapchain Configuration
	/// - Swapchain configuration is auto-determined at creation based on the capabilities of the physical
	/// device and surface
	/// - The config remains unchanged throughout the lifetime of this context
	/// - Access the configs by using @p operator->
	///   ```cpp
	///   SwapchainContext swapchain_context = ...;
	///   vk::SurfaceFormatKHR surface_format = swapchain_context->surface_format;
	///   ```
	///
	/// #### Acquiring Frames
	/// - Call @p acquire_next to acquire the next frame for rendering. This returns the acquired frame,
	/// containing index and image
	/// - Check `frame.extent_changed` to determine if the swapchain extent has changed since the last
	/// acquisition. If `true`, resources dependent on the extent should be recreated.
	/// - Access the latest extent through `swapchain_context->extent`. Note that the extent is
	/// `std::optional<glm::u32vec2>`, and will be `std::nullopt` if the swapchain is not in a valid state
	/// (e.g. before the first acquisition or after an extent change and before the next acquisition)
	///
	/// #### Presenting Frames
	/// Call @p present to present a rendered frame. See the function for details.
	///
	class SwapchainContext
	{
	  public:

		struct Frame
		{
			glm::u32vec2 extent;
			bool extent_changed;
			uint32_t index;
			vk::Image image;
			vk::ImageView image_view;
		};

		enum class Format
		{
			Srgb_8bit,
			Linear_8bit
		};

		struct Config
		{
			Format format = Format::Linear_8bit;
		};

		///
		/// @brief Create a swapchain context
		///
		/// @param instance_context Instance context, which should live longer than this context
		/// @param device_context Device context, which should live longer than this context
		/// @return Swapchain context or error
		///
		[[nodiscard]]
		static std::expected<SwapchainContext, Error> create(
			const InstanceContext& instance_context,
			const DeviceContext& device_context,
			const Config& config
		) noexcept;

		///
		/// @brief Acquire next image from the swapchain for rendering. Handles swapchain recreation
		///
		/// @note This function will block until an image is available for acquisition, or an error occurs.
		///
		/// @param instance_context Instance context
		/// @param device_context Device context
		/// @param semaphore Optional semaphore to be signaled when the image is available
		/// @param fence Optional fence to be signaled when the image is available
		/// @param timeout Optional timeout for acquisition in nanoseconds. Defaults to
		/// `std::numeric_limits<uint64_t>::max()`, i.e. no timeout
		/// @return Acquired frame or error
		///
		[[nodiscard]]
		std::expected<Frame, Error> acquire_next(
			const InstanceContext& instance_context,
			const DeviceContext& device_context,
			std::optional<vk::Semaphore> semaphore = std::nullopt,
			std::optional<vk::Fence> fence = std::nullopt,
			uint64_t timeout = std::numeric_limits<uint64_t>::max()
		) noexcept;

		///
		/// @brief Present a rendered frame. Invalidates the swapchain if presentation soft-fails
		/// (`OUT_OF_DATE` or `SUBOPTIMAL`)
		///
		/// @param device_context Device Context
		/// @param frame Frame to present, which should be acquired from this context
		/// @param wait_semaphore Optional semaphore to wait on before presentation
		/// @return Success or error. If the swapchain soft-fails, no error is returned, and the swapchain
		/// will be recreated on the next acquisition call
		///
		[[nodiscard]]
		std::expected<void, Error> present(
			const DeviceContext& device_context,
			Frame frame,
			std::optional<vk::Semaphore> wait_semaphore = std::nullopt
		) noexcept;

	  private:

		vk::SharingMode sharing_mode;
		std::unique_ptr<const std::vector<uint32_t>> queue_family_indices;
		vk::SurfaceFormatKHR surface_format;
		vk::PresentModeKHR present_mode;

		explicit SwapchainContext(
			vk::SharingMode sharing_mode,
			std::vector<uint32_t> queue_family_indices,
			vk::SurfaceFormatKHR surface_format,
			vk::PresentModeKHR present_mode
		) noexcept;

		struct RuntimeState
		{
			vk::raii::SwapchainKHR swapchain;
			glm::u32vec2 extent;

			std::vector<vk::Image> images;
			std::vector<vk::raii::ImageView> image_views;

			// First acquision call after recreating the swapchain checks and resets this flag
			bool extent_changed = true;
		};

		struct InvalidatedState
		{
			vk::raii::SwapchainKHR old_swapchain;
		};

		std::variant<std::monostate, RuntimeState, InvalidatedState> state = std::monostate();

		[[nodiscard]]
		std::expected<void, Error> check_and_recreate_swapchain(
			const InstanceContext& instance_context,
			const DeviceContext& device_context
		) noexcept;

		struct ReadonlyWrapper
		{
			vk::SharingMode sharing_mode;
			std::span<const uint32_t> queue_family_indices;
			vk::SurfaceFormatKHR surface_format;
			vk::PresentModeKHR present_mode;

			const ReadonlyWrapper* operator->() const noexcept { return this; }
		};

	  public:

		ReadonlyWrapper operator->() const noexcept;

		SwapchainContext(const SwapchainContext&) = delete;
		SwapchainContext(SwapchainContext&&) = default;
		SwapchainContext& operator=(const SwapchainContext&) = delete;
		SwapchainContext& operator=(SwapchainContext&&) = default;
	};
}

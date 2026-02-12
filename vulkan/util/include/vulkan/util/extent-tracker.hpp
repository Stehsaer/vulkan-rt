#pragma once

#include <glm/glm.hpp>
#include <optional>

namespace vulkan::util
{
	///
	/// @brief Utility class to track the changes of swapchain extent
	/// @details
	/// Use this class to track the changes of swapchain extent, to recreate frame objects when necessary
	///
	/// - Update the tracker with the new extent after acquiring swapchain image:
	///   ```cpp
	///   tracker.update(extent);
	///   ```
	/// - Check if the extent is changed before rendering:
	///   ```cpp
	///   if (tracker.is_changed())
	///   { ... }
	///   ```
	/// - If an initial extent is available, it can be passed to the constructor:
	///   ```cpp
	///   vulkan::util::ExtentTracker tracker(initial_extent);
	///   ```
	///
	class ExtentTracker
	{
	  public:

		///
		/// @brief Update the tracker with the new extent
		///
		/// @param new_extent New swapchain extent
		///
		void update(glm::u32vec2 new_extent) noexcept;

		///
		/// @brief Check if the extent is changed since the last update
		///
		/// @return `true` if the extent is changed, `false` otherwise
		///
		[[nodiscard]]
		bool is_changed() const noexcept;

		ExtentTracker() = default;

		ExtentTracker(glm::u32vec2 initial_extent) :
			previous_extent(initial_extent)
		{}

		ExtentTracker(const ExtentTracker&) = default;
		ExtentTracker(ExtentTracker&&) = default;
		ExtentTracker& operator=(const ExtentTracker&) = default;
		ExtentTracker& operator=(ExtentTracker&&) = default;

	  private:

		std::optional<glm::u32vec2> previous_extent;
		std::optional<glm::u32vec2> current_extent;
	};
}
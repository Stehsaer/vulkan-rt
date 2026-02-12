#include "vulkan/util/extent-tracker.hpp"

namespace vulkan::util
{
	void ExtentTracker::update(glm::u32vec2 new_extent) noexcept
	{
		previous_extent = current_extent;
		current_extent = new_extent;
	}

	bool ExtentTracker::is_changed() const noexcept
	{
		if (!previous_extent.has_value() || !current_extent.has_value()) return true;
		return current_extent.value() != previous_extent.value();
	}
}
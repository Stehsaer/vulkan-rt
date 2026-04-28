#pragma once

#include "render/interface/auto-exposure.hpp"

namespace logic
{
	///
	/// @brief Exposure parameters
	///
	///
	struct Exposure
	{
		float adaptation_rate = 1.0f;  // Higher value for faster adaptation
		float exposure_ev = 0.0f;      // EV value (log2 value of exposure mult)

		///
		/// @brief Configuration UI
		///
		void config_ui() noexcept;

		///
		/// @brief Get exposure parameters
		///
		/// @param dt Delta-time since last frame
		/// @param extent Swapchain extent
		/// @return Exposure parameters
		///
		[[nodiscard]]
		render::ExposureParam get(float dt, glm::u32vec2 extent) const noexcept;
	};
}

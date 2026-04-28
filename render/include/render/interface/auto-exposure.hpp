#pragma once

#include <glm/glm.hpp>

namespace render
{
	///
	/// @brief Exposure parameters
	///
	///
	struct ExposureParam
	{
		float min_log_luminance;  // Lower bound of the smallest bin
		float max_log_luminance;  // Upper bound of the largest bin
		float delta_time;         // Delta-time of the current frame
		float exposure_mult;      // Multiplier for the exposure value
		float adaptation_rate;    // Rate at which the exposure adapts to changes in scene brightness
		glm::u32vec2 image_size;  // Size of the input image for which the histogram was computed
	};

	///
	/// @brief Exposure result of current frame
	///
	///
	struct ExposureResult
	{
		float luminance_mult;  // Multiplier for the luminance values
	};

}

#pragma once

#include <glm/glm.hpp>

namespace render
{
	///
	/// @brief Internal exposure frame data
	///
	struct ExposureFrame
	{
		float mid_luminance;
	};

	///
	/// @brief Type of histogram bins
	///
	using ExposureHistogramType = uint32_t;

	///
	/// @brief Number of bins in the exposure histogram
	///
	constexpr uint32_t EXPOSURE_HISTOGRAM_BIN_COUNT = 256;
}

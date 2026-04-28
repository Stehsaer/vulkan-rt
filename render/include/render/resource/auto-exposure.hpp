#pragma once

#include "common/util/error.hpp"
#include "render/interface/auto-exposure.hpp"
#include "render/internal/auto-exposure.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/alloc/buffer.hpp"
#include "vulkan/interface/common-context.hpp"

namespace render
{
	///
	/// @brief Resources produced and internally used by auto exposure
	///
	class AutoExposureResource
	{
	  public:

		///
		/// @brief Create the auto-exposure resources
		///
		/// @param device Vulkan device context
		/// @return Created auto-exposure resource or error
		///
		[[nodiscard]]
		static std::expected<AutoExposureResource, Error> create(
			const vulkan::DeviceContext& device
		) noexcept;

		struct Ref
		{
			// Histogram buffer with `EXPOSURE_HISTOGRAM_BIN_COUNT` bins
			vulkan::ElementBufferRef<ExposureHistogramType[EXPOSURE_HISTOGRAM_BIN_COUNT]> histogram_buffer;

			// Exposure result buffer, contains the exposure result of the current frame
			vulkan::ElementBufferRef<ExposureResult> exposure_result_buffer;

			// Exposure frame buffer, contains the internal exposure frame data of the current frame
			vulkan::ElementBufferRef<ExposureFrame> exposure_frame_buffer;

			const Ref* operator->() const noexcept { return this; }
		};

		///
		/// @brief Get references to the buffers
		///
		/// @return References
		///
		[[nodiscard]]
		Ref ref() const noexcept
		{
			return {
				.histogram_buffer = histogram_buffer,
				.exposure_result_buffer = exposure_result_buffer,
				.exposure_frame_buffer = exposure_frame_buffer
			};
		}

		Ref operator->() const noexcept { return ref(); }

	  private:

		vulkan::ElementBuffer<ExposureHistogramType[EXPOSURE_HISTOGRAM_BIN_COUNT]> histogram_buffer;
		vulkan::ElementBuffer<ExposureResult> exposure_result_buffer;
		vulkan::ElementBuffer<ExposureFrame> exposure_frame_buffer;

		explicit AutoExposureResource(
			vulkan::ElementBuffer<ExposureHistogramType[EXPOSURE_HISTOGRAM_BIN_COUNT]> histogram_buffer,
			vulkan::ElementBuffer<ExposureResult> exposure_result_buffer,
			vulkan::ElementBuffer<ExposureFrame> exposure_frame_buffer
		) :
			histogram_buffer(std::move(histogram_buffer)),
			exposure_result_buffer(std::move(exposure_result_buffer)),
			exposure_frame_buffer(std::move(exposure_frame_buffer))
		{}

	  public:

		AutoExposureResource(const AutoExposureResource&) = delete;
		AutoExposureResource(AutoExposureResource&&) = default;
		AutoExposureResource& operator=(const AutoExposureResource&) = delete;
		AutoExposureResource& operator=(AutoExposureResource&&) = default;
	};
}

#include "render/resource/auto-exposure.hpp"
#include "vulkan/alloc/allocator.hpp"

namespace render
{
	std::expected<AutoExposureResource, Error> AutoExposureResource::create(
		const vulkan::DeviceContext& device
	) noexcept
	{
		auto histogram_buffer_result =
			device.allocator.create_element_buffer<ExposureHistogramType[EXPOSURE_HISTOGRAM_BIN_COUNT]>(
				vk::BufferUsageFlagBits::eStorageBuffer,
				vulkan::MemoryUsage::GpuOnly
			);

		auto exposure_result_buffer_result = device.allocator.create_element_buffer<ExposureResult>(
			vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eUniformBuffer,
			vulkan::MemoryUsage::GpuOnly
		);

		auto exposure_frame_buffer_result = device.allocator.create_element_buffer<ExposureFrame>(
			vk::BufferUsageFlagBits::eStorageBuffer,
			vulkan::MemoryUsage::GpuOnly
		);

		if (!histogram_buffer_result)
			return histogram_buffer_result.error().forward("Create histogram buffer failed");
		if (!exposure_result_buffer_result)
			return exposure_result_buffer_result.error().forward("Create exposure result buffer failed");
		if (!exposure_frame_buffer_result)
			return exposure_frame_buffer_result.error().forward("Create exposure frame buffer failed");

		return AutoExposureResource(
			std::move(*histogram_buffer_result),
			std::move(*exposure_result_buffer_result),
			std::move(*exposure_frame_buffer_result)
		);
	}
}

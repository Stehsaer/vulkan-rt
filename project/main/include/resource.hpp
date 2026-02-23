#pragma once

#include "interface/camera-param.hpp"
#include "vulkan/context/device.hpp"
#include "vulkan/util/frame-buffer.hpp"

#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

namespace resource
{
	static constexpr uint32_t inflight_frames = 3;
	static constexpr auto depth_format = vk::Format::eD32Sfloat;
	static constexpr vk::ImageUsageFlags depth_usages = {};

	struct SyncPrimitive
	{
		vk::raii::Fence draw_fence;
		vk::raii::Semaphore render_finished_semaphore;
		vk::raii::Semaphore image_available_semaphore;

		[[nodiscard]]
		static SyncPrimitive create(const vulkan::DeviceContext& context);
	};

	struct FrameResource
	{
		vulkan::FrameBuffer depth_buffer;
		interface::CameraParam::Resource camera_param;

		static FrameResource create(const vulkan::DeviceContext& context, glm::u32vec2 swapchain_extent);
	};

	struct FrameDescriptorSet
	{
		interface::CameraParam::DescriptorSet camera_param;

		void bind_resource(
			const vk::raii::Device& device,
			const FrameResource& resource,
			const FrameResource& prev_frame_resource
		) const noexcept;
	};

	struct Layout
	{
		interface::CameraParam::Layout camera_param_layout;

		[[nodiscard]]
		static Layout create(const vulkan::DeviceContext& context);
	};

	class DescriptorPool
	{
	  public:

		[[nodiscard]]
		static DescriptorPool create(
			const vk::raii::Device& device,
			const Layout& layout,
			uint32_t set_count
		);

		[[nodiscard]]
		std::vector<FrameDescriptorSet> get_frame_descriptor_sets() const;

	  private:

		interface::CameraParam::DescriptorPool camera_param_pool;

		explicit DescriptorPool(interface::CameraParam::DescriptorPool camera_param_pool) :
			camera_param_pool(std::move(camera_param_pool))
		{}

	  public:

		DescriptorPool(const DescriptorPool&) = delete;
		DescriptorPool(DescriptorPool&&) = default;
		DescriptorPool& operator=(const DescriptorPool&) = delete;
		DescriptorPool& operator=(DescriptorPool&&) = default;
	};
}

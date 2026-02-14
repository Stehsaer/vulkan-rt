#pragma once

#include "vulkan/alloc.hpp"
#include "vulkan/context/device.hpp"

#include <vulkan/vulkan_raii.hpp>

struct FrameSyncPrimitive
{
	vk::raii::Fence draw_fence;
	vk::raii::Semaphore render_finished_semaphore;
	vk::raii::Semaphore image_available_semaphore;

	static FrameSyncPrimitive create(const vulkan::DeviceContext& context);
};

struct FrameRenderResource
{
	vulkan::alloc::Image depth_buffer;
	vk::raii::ImageView depth_buffer_view;

	static FrameRenderResource create(const vulkan::DeviceContext& context, glm::u32vec2 swapchain_extent);
};

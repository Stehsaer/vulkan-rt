#pragma once

#include "vulkan/alloc.hpp"
#include "vulkan/context/device.hpp"
#include "vulkan/util/frame-buffer.hpp"

#include <vulkan/vulkan_core.h>
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
	static constexpr auto depth_format = vk::Format::eD32Sfloat;

	static constexpr vk::ImageUsageFlags depth_usages = {};

	vulkan::FrameBuffer depth_buffer;

	static FrameRenderResource create(const vulkan::DeviceContext& context, glm::u32vec2 swapchain_extent);
};

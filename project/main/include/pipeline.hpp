#pragma once

#include "vulkan/context/device.hpp"

#include <vulkan/vulkan_raii.hpp>

struct ObjectRenderPipeline
{
	vk::raii::PipelineLayout layout;
	vk::raii::Pipeline pipeline;

	struct PushConstant
	{
		glm::mat4 view_projection;
	};

	static ObjectRenderPipeline create(
		const vulkan::DeviceContext& context,
		const vk::PipelineRenderingCreateInfo& rendering_info
	);
};
#pragma once

#include "vulkan/context/device.hpp"

#include <vulkan/vulkan_raii.hpp>

struct ObjectRenderPipeline
{
	struct Parameters
	{
		glm::mat4 view_projection;
		glm::vec3 view_pos;
	};

	vk::raii::PipelineLayout layout;
	vk::raii::Pipeline pipeline;

	static ObjectRenderPipeline create(
		const vulkan::DeviceContext& context,
		const vk::PipelineRenderingCreateInfo& rendering_info
	);

	void set_params(const vk::raii::CommandBuffer& command_buffer, const Parameters& params) const noexcept;
};

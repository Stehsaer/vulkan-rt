#include "render/pipeline/util/fullscreen-pipeline.hpp"
#include "common/util/error.hpp"
#include "shader/fullscreen.hpp"
#include "vulkan/util/shader.hpp"

#include <expected>
#include <vulkan/vulkan_raii.hpp>

namespace render::fullscreen
{
	std::expected<vk::raii::ShaderModule, Error> get_vertex_shader(const vk::raii::Device& device) noexcept
	{
		return vulkan::create_shader(device, shader::fullscreen);
	}
}

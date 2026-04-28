#include "render/pipeline/util/fullscreen-pipeline.hpp"
#include "shader/fullscreen.hpp"
#include "vulkan/util/shader.hpp"

namespace render::fullscreen
{
	std::expected<vk::raii::ShaderModule, Error> get_vertex_shader(const vk::raii::Device& device) noexcept
	{
		return vulkan::create_shader(device, shader::fullscreen);
	}
}

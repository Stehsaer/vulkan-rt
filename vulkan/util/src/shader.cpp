#include "vulkan/util/shader.hpp"

namespace vulkan::util
{
	std::expected<vk::raii::ShaderModule, Error> create_shader(
		const vk::raii::Device& device,
		std::span<const std::byte> span
	) noexcept
	{
		if (span.size() % sizeof(uint32_t) != 0)
			return Error("Shader bytecode size is not a multiple of 4 bytes");

		const vk::ShaderModuleCreateInfo create_info{
			.codeSize = span.size(),
			.pCode = reinterpret_cast<const uint32_t*>(span.data()),
		};

		auto shader_module_result =
			device.createShaderModule(create_info).transform_error(Error::from<vk::Result>());
		if (!shader_module_result) return shader_module_result.error().forward("Create shader module failed");
		auto shader_module = std::move(*shader_module_result);

		return shader_module;
	}
}
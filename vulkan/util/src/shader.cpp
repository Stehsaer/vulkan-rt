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

		return device.createShaderModule(create_info).transform_error([](vk::Result result) {
			return Error(result, "Create shader module failed");
		});
	}
}
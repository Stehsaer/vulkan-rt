#include "vulkan/util/shader.hpp"
#include "common/util/error.hpp"
#include "common/util/span.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	std::expected<vk::raii::ShaderModule, Error> create_shader(
		const vk::raii::Device& device,
		std::span<const std::byte> span
	) noexcept
	{
		if (span.size() % sizeof(uint32_t) != 0)
			return Error("Shader bytecode size is not a multiple of 4 bytes");

		std::vector<uint32_t> shader_data(span.size() / sizeof(uint32_t));
		std::ranges::copy(span, util::as_writable_bytes(shader_data).begin());

		const vk::ShaderModuleCreateInfo create_info{
			.codeSize = shader_data.size() * sizeof(uint32_t),
			.pCode = shader_data.data(),
		};

		auto shader_module_result = device.createShaderModule(create_info);
		if (!shader_module_result) return Error::from(shader_module_result);
		auto shader_module = std::move(*shader_module_result);

		return shader_module;
	}
}

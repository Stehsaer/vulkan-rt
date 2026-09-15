#pragma once

#include <array>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>

namespace vulkan
{
	template <typename To>
	To to(auto) noexcept
	{
		static_assert(false, "Unsupported type conversion");
	}

	template <>
	[[nodiscard]]
	inline vk::Offset2D to(glm::i32vec2 offset) noexcept
	{
		return vk::Offset2D{.x = offset.x, .y = offset.y};
	}

	template <>
	[[nodiscard]]
	inline vk::Extent2D to(glm::u32vec2 extent) noexcept
	{
		return vk::Extent2D{.width = extent.x, .height = extent.y};
	}

	template <>
	[[nodiscard]]
	inline vk::Offset3D to(glm::i32vec2 offset) noexcept
	{
		return vk::Offset3D{.x = offset.x, .y = offset.y, .z = 0};
	}

	template <>
	[[nodiscard]]
	inline vk::Offset3D to(glm::i32vec3 offset) noexcept
	{
		return vk::Offset3D{.x = offset.x, .y = offset.y, .z = offset.z};
	}

	template <>
	[[nodiscard]]
	inline vk::Extent3D to(glm::u32vec2 extent) noexcept
	{
		return vk::Extent3D{.width = extent.x, .height = extent.y, .depth = 1};
	}

	template <>
	[[nodiscard]]
	inline vk::Extent3D to(glm::u32vec3 extent) noexcept
	{
		return vk::Extent3D{.width = extent.x, .height = extent.y, .depth = extent.z};
	}

	template <>
	[[nodiscard]]
	inline vk::TransformMatrixKHR to(glm::mat4x3 matrix) noexcept
	{
		return {
			.matrix = std::to_array({
				std::to_array<float>({matrix[0].x, matrix[1].x, matrix[2].x, matrix[3].x}),
				std::to_array<float>({matrix[0].y, matrix[1].y, matrix[2].y, matrix[3].y}),
				std::to_array<float>({matrix[0].z, matrix[1].z, matrix[2].z, matrix[3].z}),
			})
		};
	}

	template <>
	[[nodiscard]]
	inline vk::TransformMatrixKHR to(glm::mat4 matrix) noexcept
	{
		return to<vk::TransformMatrixKHR>(static_cast<glm::mat4x3>(matrix));
	}
}

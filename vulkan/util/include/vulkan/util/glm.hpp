#pragma once

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
	inline vk::Offset2D to<vk::Offset2D>(glm::i32vec2 offset) noexcept
	{
		return vk::Offset2D{.x = offset.x, .y = offset.y};
	}

	template <>
	[[nodiscard]]
	inline vk::Extent2D to<vk::Extent2D>(glm::u32vec2 extent) noexcept
	{
		return vk::Extent2D{.width = extent.x, .height = extent.y};
	}

	template <>
	[[nodiscard]]
	inline vk::Offset3D to<vk::Offset3D>(glm::i32vec2 offset) noexcept
	{
		return vk::Offset3D{.x = offset.x, .y = offset.y, .z = 0};
	}

	template <>
	[[nodiscard]]
	inline vk::Offset3D to<vk::Offset3D>(glm::i32vec3 offset) noexcept
	{
		return vk::Offset3D{.x = offset.x, .y = offset.y, .z = offset.z};
	}

	template <>
	[[nodiscard]]
	inline vk::Extent3D to<vk::Extent3D>(glm::u32vec2 extent) noexcept
	{
		return vk::Extent3D{.width = extent.x, .height = extent.y, .depth = 1};
	}

	template <>
	[[nodiscard]]
	inline vk::Extent3D to<vk::Extent3D>(glm::u32vec3 extent) noexcept
	{
		return vk::Extent3D{.width = extent.x, .height = extent.y, .depth = extent.z};
	}

}

#pragma once

#include "common/container/error.hpp"
#include "vulkan/common/context.hpp"

#include <expected>
#include <set>
#include <string>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan::platform
{
	///
	/// @brief Get instance layers given specific feature
	///
	/// @param feature Instance features
	/// @return Set of layer strings
	///
	[[nodiscard]]
	std::set<std::string> get_instance_layers(const InstanceFeature& feature) noexcept;

	///
	/// @brief Get instance extensions given specific feature
	///
	/// @param feature Instance features
	/// @return Set of extension strings
	///
	[[nodiscard]]
	std::set<std::string> get_instance_extensions(const InstanceFeature& feature) noexcept;

	///
	/// @brief Create a vulkan instance
	///
	/// @param context Vulkan primary context
	/// @param property Instance property
	/// @param requested_layers Requested layers, obtained by `get_instance_layers`
	/// @param requested_extensions Requested extensions, obtained by `get_instance_extensions`
	/// @return
	///
	[[nodiscard]]
	std::expected<vk::raii::Instance, Error> create_instance(
		const vk::raii::Context& context,
		const InstanceProperty& property,
		const std::set<std::string>& requested_layers,
		const std::set<std::string>& requested_extensions
	) noexcept;
}

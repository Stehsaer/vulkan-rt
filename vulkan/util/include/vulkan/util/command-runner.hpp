#pragma once

#include "common/util/error.hpp"
#include "vulkan/interface/context.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <utility>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief Simple command runner, contains all necessary stuffs to run commands on GPU easily
	/// @note Not suitable for render-loop, as it frequently allocates and deallocates command buffers
	///
	class CommandRunner
	{
	  public:

		///
		/// @brief Create a command runner
		///
		/// @param context Vulkan context
		/// @return Created command runner or error
		///
		static std::expected<CommandRunner, Error> create(const vulkan::Context& context) noexcept;

		///
		/// @brief Run given action
		///
		/// @param context Vulkan context
		/// @param action Action, returns void
		/// @param timeout Timeout for waiting on fence
		/// @return `void` if success or error
		///
		std::expected<void, Error> run(
			const vulkan::Context& context,
			const std::function<void(const vk::raii::CommandBuffer& command_buffer)>& action,
			uint64_t timeout = std::numeric_limits<uint64_t>::max()
		) const noexcept;

	  private:

		vk::raii::CommandPool command_pool;
		vk::raii::Fence fence;

		explicit CommandRunner(vk::raii::CommandPool command_pool, vk::raii::Fence fence) :
			command_pool(std::move(command_pool)),
			fence(std::move(fence))
		{}

	  public:

		CommandRunner(const CommandRunner&) = delete;
		CommandRunner(CommandRunner&&) = default;
		CommandRunner& operator=(const CommandRunner&) = delete;
		CommandRunner& operator=(CommandRunner&&) = default;
	};
}

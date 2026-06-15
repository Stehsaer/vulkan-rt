#pragma once

#include "common/util/error.hpp"
#include "vulkan/interface/context.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <limits>
#include <mutex>
#include <utility>
#include <vulkan/vulkan.hpp>
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

		///
		/// @brief Run given action with result
		///
		/// @tparam T Type of the return value
		/// @param context Vulkan context
		/// @param action Action, returns void
		/// @param timeout Timeout for waiting on fence
		/// @return `void` if success or error
		///
		template <typename T>
		std::expected<T, Error> run(
			const vulkan::Context& context,
			const std::function<std::expected<T, Error>(const vk::raii::CommandBuffer& command_buffer)>&
				action,
			uint64_t timeout = std::numeric_limits<uint64_t>::max()
		) const noexcept;

	  private:

		vk::raii::CommandPool command_pool;

		explicit CommandRunner(vk::raii::CommandPool command_pool) :
			command_pool(std::move(command_pool))
		{}

	  public:

		CommandRunner(const CommandRunner&) = delete;
		CommandRunner(CommandRunner&&) = default;
		CommandRunner& operator=(const CommandRunner&) = delete;
		CommandRunner& operator=(CommandRunner&&) = default;
	};

	template <typename T>
	std::expected<T, Error> CommandRunner::run(
		const vulkan::Context& context,
		const std::function<std::expected<T, Error>(const vk::raii::CommandBuffer& command_buffer)>& action,
		uint64_t timeout
	) const noexcept
	{
		auto command_buffer_result = context.device.allocateCommandBuffers({
			.commandPool = command_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 1,
		});
		if (!command_buffer_result) return Error::from(command_buffer_result);
		auto command_buffer = std::move(command_buffer_result->at(0));

		auto fence_result = context.device.createFence({});
		if (!fence_result) return Error::from(fence_result);
		auto fence = std::move(*fence_result);

		if (const auto result = command_buffer.begin({}); !result) return Error::from(result);

		auto return_value = action(command_buffer);

		if (const auto result = command_buffer.end(); !result) return Error::from(result);

		const auto command_buffer_info = vk::CommandBufferSubmitInfo{.commandBuffer = command_buffer};
		const auto submit_info = vk::SubmitInfo2().setCommandBufferInfos(command_buffer_info);

		{
			const std::scoped_lock lock(context.submit_mutex);

			if (const auto submit_result = context.queue.submit2(submit_info, fence); !submit_result)
				return Error::from(submit_result);
		}

		if (const auto result = context.device.waitForFences({fence}, vk::True, timeout);
			result != vk::Result::eSuccess)
			return Error::from(result);

		if (const auto result = context.device.resetFences({fence}); !result) return Error::from(result);

		return return_value;
	}
}

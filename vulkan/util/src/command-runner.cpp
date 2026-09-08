#include "vulkan/util/command-runner.hpp"
#include "common/util/error.hpp"
#include "vulkan/interface/context.hpp"

#include <cstdint>
#include <expected>
#include <functional>
#include <mutex>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	std::expected<CommandRunner, Error> CommandRunner::create(const vulkan::Context& context) noexcept
	{
		auto command_pool_result = context.device.createCommandPool({
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = context.family,
		});
		if (!command_pool_result) return Error::from(command_pool_result);
		auto command_pool = std::move(*command_pool_result);

		return CommandRunner(std::move(command_pool));
	}

	std::expected<void, Error> CommandRunner::run(
		const vulkan::Context& context,
		const std::function<void(const vk::raii::CommandBuffer& command_buffer)>& action,
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

		action(command_buffer);

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

		return {};
	}
}

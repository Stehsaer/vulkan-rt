#pragma once

#include "common/util/tagged-type.hpp"
#include "resource/context.hpp"
#include "resource/sync-primitive.hpp"
#include "vulkan/context/swapchain.hpp"
#include "vulkan/util/cycle.hpp"

#include <SDL3/SDL_events.h>
#include <vulkan/vulkan_raii.hpp>

namespace helper
{
	///
	/// @brief Helper class for drawing a imgui-only page
	///
	class ImGuiPage
	{
	  public:

		///
		/// @brief Result states
		///
		enum class ResultState
		{
			Success,  // No error, continue to execute
			Wait,     // Waiting for swapchain to be available
			Quit,     // Quit has been requested from events
			Error     // Error occurred
		};

		template <typename T>
		using Result = util::EnumVariant<
			util::Tag<ResultState::Success, util::TagVoidlessType<T>>,
			util::Tag<ResultState::Wait>,
			util::Tag<ResultState::Quit>,
			util::Tag<ResultState::Error, Error>
		>;

		///
		/// @brief Create an imgui page
		///
		/// @param context Vulkan context
		/// @return Created imgui page or error
		///
		static std::expected<ImGuiPage, Error> create(const vulkan::DeviceContext& context) noexcept;

		///
		/// @brief Run frame
		///
		/// @tparam Func Type of UI function
		/// @param context Vulkan context
		/// @param ui_func UI function, called to get UI contents with no arguments
		///
		/// @retval ResultState::Success Return value of `ui_func`
		/// @retval ResultState::Wait Swapchain not available, `ui_func` hasn't been executed
		/// @retval ResultState::Quit Quit has been requested
		/// @retval ResultState::Error Error occurred
		///
		template <typename Func>
		auto run_frame(resource::Context& context, Func&& ui_func) noexcept
			-> Result<std::invoke_result_t<Func>>
		{
			using ReturnType = Result<std::invoke_result_t<Func>>;

			if (!poll_events(context)) return ReturnType::template from<ResultState::Quit>();

			auto frame_context_result = prepare_frame(context);
			if (!frame_context_result)
			{
				return ReturnType::template from<ResultState::Error>(
					frame_context_result.error().forward("Prepare frame failed")
				);
			}
			if (!*frame_context_result) return ReturnType::template from<ResultState::Wait>();
			const auto frame_context = **frame_context_result;

			if constexpr (std::is_void_v<std::invoke_result_t<Func>>)
			{
				const auto new_frame_result = context.imgui.new_frame();
				if (!new_frame_result)
				{
					return ReturnType::template from<ResultState::Error>(
						new_frame_result.error().forward("Start new ImGui frame failed")
					);
				}

				ui_func();

				const auto render_result = context.imgui.render();
				if (!render_result)
				{
					return ReturnType::template from<ResultState::Error>(
						render_result.error().forward("Render ImGui frame failed")
					);
				}

				auto draw_result = draw_frame(context, frame_context);
				if (!draw_result)
				{
					return ReturnType::template from<ResultState::Error>(
						draw_result.error().forward("Draw frame failed")
					);
				}

				return ReturnType::template from<ResultState::Success>();
			}
			else
			{
				const auto new_frame_result = context.imgui.new_frame();
				if (!new_frame_result)
				{
					return ReturnType::template from<ResultState::Error>(
						new_frame_result.error().forward("Start new ImGui frame failed")
					);
				}

				auto ui_result = ui_func();

				const auto render_result = context.imgui.render();
				if (!render_result)
				{
					return ReturnType::template from<ResultState::Error>(
						render_result.error().forward("Render ImGui frame failed")
					);
				}

				auto draw_result = draw_frame(context, frame_context);
				if (!draw_result)
				{
					return ReturnType::template from<ResultState::Error>(
						draw_result.error().forward("Draw frame failed")
					);
				}

				return ReturnType::template from<ResultState::Success>(std::move(ui_result));
			}
		}

	  private:

		struct FrameContext
		{
			const vk::raii::CommandBuffer& command_buffer;
			const resource::SyncPrimitive& sync;
			vulkan::SwapchainContext::Frame swapchain;
		};

		vk::raii::CommandPool command_pool;
		vulkan::Cycle<vk::raii::CommandBuffer> command_buffers;
		vulkan::Cycle<resource::SyncPrimitive> sync_primitives;

		[[nodiscard]]
		bool poll_events(resource::Context& context) const noexcept;

		[[nodiscard]]
		std::expected<std::optional<FrameContext>, Error> prepare_frame(resource::Context& context) noexcept;

		[[nodiscard]]
		std::expected<void, Error> draw_frame(
			resource::Context& context,
			const FrameContext& frame_context
		) noexcept;

		explicit ImGuiPage(
			vk::raii::CommandPool command_pool,
			vulkan::Cycle<vk::raii::CommandBuffer> command_buffers,
			vulkan::Cycle<resource::SyncPrimitive> sync_primitives
		) :
			command_pool(std::move(command_pool)),
			command_buffers(std::move(command_buffers)),
			sync_primitives(std::move(sync_primitives))
		{}

	  public:

		ImGuiPage(const ImGuiPage&) = delete;
		ImGuiPage(ImGuiPage&&) = default;
		ImGuiPage& operator=(const ImGuiPage&) = delete;
		ImGuiPage& operator=(ImGuiPage&&) = default;
	};
}

#pragma once

#include "common/util/error.hpp"
#include "vulkan/context/device.hpp"
#include "vulkan/context/instance.hpp"

#include <imgui_impl_sdl3.h>
#include <vulkan/vulkan_handles.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace vulkan
{
	///
	/// @brief Context for ImGui UI system
	/// @details
	/// #### Creation
	/// To create an ImGui context, call @p create with context and configuration. For configuration, see doc
	/// for @p Config
	///
	/// #### Usage
	/// 1. Process SDL events with `process_event`
	/// 2. Start a new frame with `new_frame`
	/// 3. Draw UI with ImGui functions
	/// 4. Render the ImGui draw data with `render`
	/// 5. Inside a command buffer, record ImGui draw commands with `draw`:
	///   - For dynamic rendering, make sure to record inside the dynamic rendering scope
	///   - For traditional rendering, make sure to record inside a render pass instance with compatible
	///   render pass
	///
	/// @note Rendering multiple times within one frame is currently not supported.
	///
	class ImGuiContext
	{
	  public:

		struct Config
		{
			struct DynamicRendering
			{
				vk::PipelineRenderingCreateInfo rendering_info;
			};

			struct TraditionalRendering
			{
				vk::RenderPass render_pass;
				uint32_t subpass_index;
				vk::SampleCountFlagBits sample_count;
			};

			///
			/// @brief ImGui rendering scheme
			/// @details
			/// - For dynamic rendering, fill in the @p DynamicRendering struct and set @p render_scheme to it
			/// - For traditional rendering, fill in the @p TraditionalRendering struct and set @p
			/// render_scheme to it
			///
			std::variant<DynamicRendering, TraditionalRendering> render_scheme;
		};

		///
		/// @brief Create an ImGui context
		///
		/// @param instance_context Instance context
		/// @param device_context Device context
		/// @param config Configuration for ImGui context creation
		/// @return Created ImGui context or error
		///
		[[nodiscard]]
		static std::expected<ImGuiContext, Error> create(
			const InstanceContext& instance_context,
			const DeviceContext& device_context,
			const Config& config
		) noexcept;

		///
		/// @brief Process an SDL event for ImGui
		///
		/// @param event SDL event to process
		///
		void process_event(const SDL_Event& event) noexcept;

		///
		/// @brief Start a new frame for ImGui
		/// @note This function has builtin state checking. If in wrong state, this function fails with error
		/// @return `void` on success or error
		///
		std::expected<void, Error> new_frame() noexcept;

		///
		/// @brief Render the ImGui draw data
		/// @note This function has builtin state checking. If in wrong state, this function fails with error
		/// @return `void` on success or error
		///
		std::expected<void, Error> render() noexcept;

		///
		/// @brief Record ImGui draw commands into the given command buffer
		/// @note This function has builtin state checking. If in wrong state, this function fails with error
		/// @param command_buffer Command buffer to record draw commands into
		/// @return `void` on success or error
		///
		std::expected<void, Error> draw(const vk::raii::CommandBuffer& command_buffer) noexcept;

	  private:

		struct ContextDeleter
		{
			~ContextDeleter() noexcept;
		};

		enum class State
		{
			Idle,      // Idle state before starting a new frame
			Logic,     // New frame started, drawing UI, but not yet rendered
			Complete,  // UI rendered, but not yet presented
		};

		std::unique_ptr<ContextDeleter> deleter = std::make_unique<ContextDeleter>();
		State state = State::Idle;

		explicit ImGuiContext() = default;

		static void check_imgui_vk_result(VkResult err);

	  public:

		ImGuiContext(const ImGuiContext&) = delete;
		ImGuiContext(ImGuiContext&&) = default;
		ImGuiContext& operator=(const ImGuiContext&) = delete;
		ImGuiContext& operator=(ImGuiContext&&) = default;
	};
}

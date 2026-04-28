#pragma once

#include "common/util/error.hpp"
#include "render/interface/auto-exposure.hpp"
#include "render/interface/camera.hpp"
#include "render/resource/auto-exposure.hpp"
#include "render/resource/forward-rendering.hpp"
#include "render/resource/host.hpp"
#include "render/resource/indirect.hpp"
#include "render/util/per-render-state.hpp"
#include "vulkan/interface/common-context.hpp"

#include <vulkan/vulkan_raii.hpp>

namespace resource
{
	///
	/// @brief Render data needed for a frame
	///
	struct RenderData
	{
		// Drawcalls for different render modes
		render::PerRenderState<std::span<const render::PrimitiveDrawcall>> drawcalls;

		// Transform matrices for all nodes
		std::span<const glm::mat4> transforms;

		render::Camera camera;                 // Camera parameters
		render::DirectLight primary_light;     // Primary light parameters
		render::ExposureParam exposure_param;  // Exposure parameters
	};

	///
	/// @brief Render resources
	/// @note See the documentation of classes of each member for more detail
	///
	struct RenderResource
	{
		render::HostParamResource param;
		render::HostDrawcallResource drawcall;
		render::IndirectResource indirect;
		render::ForwardRenderResource forward;
		render::AutoExposureResource auto_exposure;

		///
		/// @brief Create render resources
		///
		/// @param context Vulkan context
		/// @return Created render resource or error
		///
		[[nodiscard]]
		static std::expected<RenderResource, Error> create(const vulkan::DeviceContext& context) noexcept;

		///
		/// @brief Update render resource
		///
		/// @param context Vulkan context
		/// @param data Render data input
		/// @return `void` if success, or error
		///
		[[nodiscard]]
		std::expected<void, Error> update(
			const vulkan::DeviceContext& context,
			const RenderData& data
		) noexcept;

		///
		/// @brief Resize the render resources (typically the render targets)
		///
		/// @param context Vulkan context
		/// @param extent Swapchain extent
		/// @return `void` if success, or error
		///
		[[nodiscard]]
		std::expected<void, Error> resize(const vulkan::DeviceContext& context, glm::u32vec2 extent) noexcept;

		///
		/// @brief Check if all render targets in the resources are complete (ready to use)
		///
		/// @return `true` for complete, `false` otherwise
		///
		[[nodiscard]]
		bool is_complete() const noexcept;

		///
		/// @brief Record upload and barrier commands
		///
		/// @param command_buffer Command buffer
		///
		void upload(const vk::raii::CommandBuffer& command_buffer) const noexcept;
	};
}

#pragma once

#include "common/util/error.hpp"
#include "logic/drawcall-gen.hpp"
#include "logic/param.hpp"
#include "render/interface/auto-exposure.hpp"
#include "render/interface/camera.hpp"
#include "render/interface/direct-light.hpp"
#include "render/interface/primitive-drawcall.hpp"
#include "render/model/material.hpp"
#include "render/model/model.hpp"
#include "render/model/tlas.hpp"
#include "render/resource/raytrace.hpp"
#include "render/util/per-render-state.hpp"
#include "resource/aux-resource.hpp"
#include "resource/context.hpp"
#include "resource/pipeline.hpp"
#include "resource/render-resource.hpp"
#include "resource/sync-primitive.hpp"
#include "scene/page.hpp"
#include "vulkan/container/host/cycle.hpp"
#include "vulkan/context/swapchain.hpp"

#include <cstddef>
#include <expected>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/ext/vector_uint2_sized.hpp>
#include <memory>
#include <optional>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace page
{
	///
	/// @brief Main render page
	///
	///
	class RenderPage : public scene::Page
	{
	  public:

		[[nodiscard]]
		std::expected<ResultType, Error> run_frame() noexcept override;

		///
		/// @brief Create a render page
		///
		/// @param context Vulkan context
		/// @param model Model instance
		/// @param material_layout Material layout for model
		/// @return Created render page or error
		///
		[[nodiscard]]
		static std::expected<RenderPage, Error> create(
			std::shared_ptr<resource::Context> context,
			render::MaterialLayout material_layout,
			render::Model model,
			render::Tlas tlas
		) noexcept;

	  private:

		/* Struct Definition */

		struct FrameResource
		{
			vk::raii::CommandBuffer command_buffer;
			resource::RenderResource render_resource;
			resource::ResourceSet resource_set;
			resource::FrameSyncPrimitive sync_primitive;

			explicit FrameResource(
				vk::raii::CommandBuffer command_buffer,
				resource::RenderResource render_resource,
				resource::ResourceSet resource_set,
				resource::FrameSyncPrimitive sync_primitive
			) :
				command_buffer(std::move(command_buffer)),
				render_resource(std::move(render_resource)),
				resource_set(std::move(resource_set)),
				sync_primitive(std::move(sync_primitive))
			{}

			FrameResource(const FrameResource&) = delete;
			FrameResource(FrameResource&&) = default;
			FrameResource& operator=(const FrameResource&) = delete;
			FrameResource& operator=(FrameResource&&) = default;
		};

		struct Frame
		{
			const vk::raii::CommandBuffer& command_buffer;
			const resource::RenderResource& render_resource;
			const resource::RenderResource& prev_render_resource;
			const resource::ResourceSet& resource_set;
			const resource::FrameSyncPrimitive& sync_primitive;
			vk::Semaphore render_complete_semaphore;
			vulkan::SwapchainContext::Frame swapchain;
		};

		struct SceneData
		{
			render::PerRenderState<std::pmr::vector<render::PrimitiveDrawcall>> drawcalls;
			std::pmr::vector<glm::mat4> transforms;
			render::Camera camera;
			render::DirectLight primary_light;
			render::ExposureParam exposure_param;

			operator resource::RenderData() const noexcept;
		};

		enum class Event
		{
			None,
			Quit
		};

		/* Graphic */

		std::shared_ptr<resource::Context> context;
		vk::raii::CommandPool command_pool;

		render::MaterialLayout material_layout;
		render::Model model;
		render::Tlas tlas;

		render::RaytraceResourceLayout raytrace_res_layout;
		render::RaytraceResource raytrace_resource;
		resource::AuxResource aux_resource;
		size_t frames = 0;

		resource::Pipeline pipeline;
		vulkan::Cycle<FrameResource> frame_resources;
		std::vector<vk::raii::Semaphore> render_complete_semaphores;  // Indexed by swapchain image indices

		logic::DrawcallGenerator drawcall_generator;
		logic::Param param = {};

		void ui(glm::u32vec2 extent) noexcept;

		[[nodiscard]]
		Event handle_events() noexcept;

		/*===== Prepare =====*/

		struct FrameAcquireResult
		{
			FrameResource &curr_resource, &prev_resource;
			vulkan::SwapchainContext::Frame swapchain_frame;
		};

		[[nodiscard]]
		std::expected<std::optional<FrameAcquireResult>, Error> acquire_frame() noexcept;

		[[nodiscard]]
		SceneData prepare_scene(glm::u32vec2 extent) noexcept;

		[[nodiscard]]
		std::expected<std::optional<Frame>, Error> prepare_frame() noexcept;

		/*===== Render =====*/

		void render_objects(const Frame& frame) const noexcept;
		void render_lighting(const Frame& frame) const noexcept;
		void render_post_processing(const Frame& frame) const noexcept;

		[[nodiscard]]
		std::expected<void, Error> render_composite(const Frame& frame) noexcept;

		[[nodiscard]]
		std::expected<void, Error> draw_frame(const Frame& frame) noexcept;

		/*===== Present =====*/

		[[nodiscard]]
		std::expected<void, Error> present_frame(const Frame& frame) noexcept;

		/* Constructor */

		explicit RenderPage(
			std::shared_ptr<resource::Context> context,
			vk::raii::CommandPool command_pool,
			render::MaterialLayout material_layout,
			render::Model model,
			render::Tlas tlas,
			render::RaytraceResourceLayout raytrace_res_layout,
			render::RaytraceResource raytrace_resource,
			resource::AuxResource aux_resource,
			resource::Pipeline pipeline,
			vulkan::Cycle<FrameResource> frame_resources,
			std::vector<vk::raii::Semaphore> render_complete_semaphores
		) :
			context(std::move(context)),
			command_pool(std::move(command_pool)),
			material_layout(std::move(material_layout)),
			model(std::move(model)),
			tlas(std::move(tlas)),
			raytrace_res_layout(std::move(raytrace_res_layout)),
			raytrace_resource(std::move(raytrace_resource)),
			aux_resource(std::move(aux_resource)),
			pipeline(std::move(pipeline)),
			frame_resources(std::move(frame_resources)),
			render_complete_semaphores(std::move(render_complete_semaphores))
		{}

	  public:

		RenderPage(const RenderPage&) = delete;
		RenderPage(RenderPage&&) = default;
		RenderPage& operator=(const RenderPage&) = delete;
		RenderPage& operator=(RenderPage&&) = default;
	};
}

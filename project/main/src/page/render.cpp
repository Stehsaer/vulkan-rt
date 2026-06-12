#include "page/render.hpp"
#include "common/number-literals.hpp"
#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "config.hpp"
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
#include "vulkan/container/host/cycle.hpp"
#include "vulkan/numeric/base-level.hpp"
#include "vulkan/numeric/glm.hpp"

#include <SDL3/SDL_events.h>
#include <cstdint>
#include <expected>
#include <format>
#include <glm/ext/matrix_float4x4.hpp>
#include <glm/ext/vector_int2_sized.hpp>
#include <glm/ext/vector_uint2_sized.hpp>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <memory>
#include <optional>
#include <random>
#include <ranges>
#include <span>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace page
{
	std::expected<RenderPage, Error> RenderPage::create(
		std::shared_ptr<resource::Context> context,
		render::MaterialLayout material_layout,
		render::Model model,
		render::Tlas tlas
	) noexcept
	{
		auto command_pool_result = context->device->createCommandPool({
			.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			.queueFamilyIndex = context->device.get().family,
		});
		if (!command_pool_result) return Error::from(command_pool_result);
		auto command_pool = std::move(*command_pool_result);

		auto command_buffers_result = context->device->allocateCommandBuffers({
			.commandPool = command_pool,
			.commandBufferCount = config::INFLIGHT_FRAMES,
		});
		if (!command_buffers_result) return Error::from(command_buffers_result);
		auto command_buffers = std::move(*command_buffers_result);

		auto raytrace_res_layout_result = render::RaytraceResourceLayout::create(context->device.get());
		if (!raytrace_res_layout_result)
			return raytrace_res_layout_result.error().forward("Create raytrace resource layout failed");
		auto raytrace_res_layout = std::move(*raytrace_res_layout_result);

		auto raytrace_resource_result =
			render::RaytraceResource::create(context->device.get(), raytrace_res_layout, model);
		if (!raytrace_resource_result)
			return raytrace_resource_result.error().forward("Create raytrace resource failed");
		auto raytrace_resource = std::move(*raytrace_resource_result);

		auto render_buffers_result =
			std::views::repeat(resource::RenderResource::create, config::INFLIGHT_FRAMES)
			| std::views::transform([&context](auto f) { return f(context->device.get()); })
			| Error::collect();
		if (!render_buffers_result)
			return render_buffers_result.error().forward("Create render buffers failed");
		auto render_buffers = std::move(*render_buffers_result);

		auto sync_primitives_result =
			std::views::repeat(resource::FrameSyncPrimitive::create, config::INFLIGHT_FRAMES)
			| std::views::transform([&context](auto f) { return f(context->device.get()); })
			| Error::collect();
		if (!sync_primitives_result)
			return sync_primitives_result.error().forward("Create sync primitives failed");
		auto sync_primitives = std::move(*sync_primitives_result);

		auto pipeline_result = resource::Pipeline::create(
			context->device.get(),
			material_layout,
			raytrace_res_layout,
			context->swapchain->surface_format.format
		);
		if (!pipeline_result) return pipeline_result.error().forward("Create pipelines failed");
		auto pipeline = std::move(*pipeline_result);

		auto resource_sets_result =
			pipeline.create_resource_sets(context->device.get(), config::INFLIGHT_FRAMES);
		if (!resource_sets_result) return resource_sets_result.error().forward("Create resource sets failed");
		auto resource_sets = std::move(*resource_sets_result);

		auto frame_resources =
			std::views::zip_transform(
				CTOR_LAMBDA(FrameResource),
				command_buffers | std::views::as_rvalue,
				render_buffers | std::views::as_rvalue,
				resource_sets | std::views::as_rvalue,
				sync_primitives | std::views::as_rvalue
			)
			| vulkan::Cycle<FrameResource>::into;

		auto render_complete_semaphores_result =
			std::views::repeat(
				[&context] { return context->device->createSemaphore({}); },
				context->swapchain->image_count
			)
			| std::views::transform([](const auto& lambda) {
				  return lambda().transform_error([](vk::Result result) { return Error::from(result); });
			  })
			| Error::collect();
		if (!render_complete_semaphores_result)
			return render_complete_semaphores_result.error().forward(
				"Create swapchain image semaphores failed"
			);
		auto render_complete_semaphores = std::move(*render_complete_semaphores_result);

		auto aux_resource_result = resource::AuxResource::create(context->device.get());
		if (!aux_resource_result)
			return aux_resource_result.error().forward("Create auxiliary resources failed");
		auto aux_resource = std::move(*aux_resource_result);

		return RenderPage(
			std::move(context),
			std::move(command_pool),
			std::move(material_layout),
			std::move(model),
			std::move(tlas),
			std::move(raytrace_res_layout),
			std::move(raytrace_resource),
			std::move(aux_resource),
			std::move(pipeline),
			std::move(frame_resources),
			std::move(render_complete_semaphores)
		);
	}

	std::expected<RenderPage::ResultType, Error> RenderPage::run_frame() noexcept
	{
		const auto event = handle_events();
		if (event == Event::Quit)
		{
			if (const auto result = context->device->waitIdle(); !result) return Error::from(result);
			return ResultType::from<Result::Quit>();
		}

		auto prepare_frame_result = prepare_frame();
		if (!prepare_frame_result)
		{
			if (const auto result = context->device->waitIdle(); !result) return Error::from(result);
			return prepare_frame_result.error().forward("Prepare frame failed");
		}
		if (!*prepare_frame_result) return ResultType::from<Result::Continue>();
		auto frame = **prepare_frame_result;

		if (const auto draw_result = draw_frame(frame); !draw_result)
		{
			if (const auto result = context->device->waitIdle(); !result) return Error::from(result);
			return draw_result.error().forward("Draw frame failed");
		}

		if (const auto present_result = present_frame(frame); !present_result)
		{
			if (const auto result = context->device->waitIdle(); !result) return Error::from(result);
			return present_result.error().forward("Present frame failed");
		}

		return ResultType::from<Result::Continue>();
	}

	std::expected<std::optional<RenderPage::FrameAcquireResult>, Error> RenderPage::acquire_frame() noexcept
	{
		/* Cycle & Wait */

		frame_resources.cycle();

		if (const auto wait_result = context->device->waitForFences(
				*frame_resources.current().sync_primitive.draw_fence,
				vk::True,
				UINT64_MAX
			);
			wait_result != vk::Result::eSuccess)
			return Error::from(wait_result);

		auto& curr_resource = frame_resources.current();
		auto& prev_resource = frame_resources.prev();

		/* Acquire Swapchain */

		const auto swapchain_result = context->swapchain.acquire_next(
			context->instance,
			context->device,
			curr_resource.sync_primitive.image_available_semaphore
		);
		if (!swapchain_result) return swapchain_result.error().forward("Acquire next swapchain image failed");
		if (!*swapchain_result) return std::nullopt;  // Soft failed, retry next frame
		const auto swapchain_frame = **swapchain_result;

		/* Check, recreate attachments if needed */

		if (swapchain_frame.extent_changed || !curr_resource.render_resource.attachments)
		{
			if (const auto result = context->device->waitIdle(); !result) return Error::from(result);

			// NOTE: recreating every set is intended
			for (auto& resource : frame_resources.iterate())
			{
				auto render_target_result = resource.render_resource.resize_attachments(
					context->device.get(),
					swapchain_frame.extent
				);
				if (!render_target_result)
					return render_target_result.error().forward("Create render target failed");
			}
		}

		return FrameAcquireResult{
			.curr_resource = curr_resource,
			.prev_resource = prev_resource,
			.swapchain_frame = swapchain_frame,
		};
	}

	RenderPage::SceneData::operator resource::RenderData() const noexcept
	{
		return {
			.drawcalls = drawcalls.to<std::span<const render::PrimitiveDrawcall>>(),
			.transforms = transforms,
			.camera = camera,
			.primary_light = primary_light,
			.exposure_param = exposure_param
		};
	}

	RenderPage::SceneData RenderPage::prepare_scene(glm::u32vec2 extent) noexcept
	{
		auto [drawcalls, transforms] = drawcall_generator.compute(model, glm::mat4(1.0f));
		const auto camera = param.camera.get_and_update(extent);
		const auto primary_light = param.primary_light.get();
		const auto exposure_param = param.exposure.get(ImGui::GetIO().DeltaTime, extent);

		return {
			.drawcalls = std::move(drawcalls),
			.transforms = std::move(transforms),
			.camera = camera,
			.primary_light = primary_light,
			.exposure_param = exposure_param
		};
	}

	void RenderPage::ui(glm::u32vec2 extent) noexcept
	{
		auto& io = ImGui::GetIO();

		auto background_drawlist = ImGui::GetBackgroundDrawList();
		const auto fps_text = std::format("{:.1f} FPS", io.Framerate);

		background_drawlist->AddText({12, 12}, IM_COL32(0, 0, 0, 255), fps_text.c_str());
		background_drawlist->AddText({10, 10}, IM_COL32(255, 255, 255, 255), fps_text.c_str());

		param.ui(extent);
	}

	RenderPage::Event RenderPage::handle_events() noexcept
	{
		SDL_Event event;

		while (SDL_PollEvent(&event))
		{
			context->imgui.process_event(event);

			switch (event.type)
			{
			case SDL_EVENT_QUIT:
				return Event::Quit;
			default:
				break;
			}
		}

		return Event::None;
	}

	std::expected<std::optional<RenderPage::Frame>, Error> RenderPage::prepare_frame() noexcept
	{
		const auto acquire_result = acquire_frame();
		if (!acquire_result) return acquire_result.error().forward("Acquire frame failed");
		if (!*acquire_result) return std::nullopt;  // Soft failed, retry next frame
		const auto frame = **acquire_result;

		/* UI & Scene */

		if (const auto new_frame_result = context->imgui.new_frame(); !new_frame_result)
			return new_frame_result.error().forward("Start new ImGui frame failed");

		ui(frame.swapchain_frame.extent);

		if (const auto render_result = context->imgui.render(); !render_result)
			return render_result.error().forward("Render ImGui frame failed");

		auto scene_data = prepare_scene(frame.swapchain_frame.extent);

		/* Update & Bind */

		if (const auto buffer_update_result =
				frame.curr_resource.render_resource.update(context->device.get(), scene_data);
			!buffer_update_result)
			return buffer_update_result.error().forward("Update render buffer failed");

		std::uniform_int_distribution<int32_t> dist(-128_i32, 128_i32);
		const auto noise_offset = glm::i32vec2(dist(random_source), dist(random_source));

		frame.curr_resource.resource_set.update(
			context->device.get(),
			model,
			tlas,
			raytrace_resource,
			frame.curr_resource.render_resource,
			frame.prev_resource.render_resource,
			aux_resource,
			noise_offset
		);

		return Frame{
			.command_buffer = frame.curr_resource.command_buffer,
			.render_resource = frame.curr_resource.render_resource,
			.prev_render_resource = frame.prev_resource.render_resource,
			.resource_set = frame.curr_resource.resource_set,
			.sync_primitive = frame.curr_resource.sync_primitive,
			.render_complete_semaphore = render_complete_semaphores[frame.swapchain_frame.index],
			.swapchain = frame.swapchain_frame,
		};
	}

	void RenderPage::render_objects(const Frame& frame) const noexcept
	{
		pipeline.indirect.compute(frame.command_buffer, frame.resource_set.indirect);
		pipeline.deferred.render(frame.command_buffer, frame.resource_set.deferred);
		pipeline.downsample.downsample(frame.command_buffer, frame.resource_set.downsample);
		pipeline.shadow_trace.trace(frame.command_buffer, frame.resource_set.shadow_trace);
	}

	void RenderPage::render_lighting(const Frame& frame) const noexcept
	{
		const auto rendering_area = vk::Rect2D{
			.offset = vk::Offset2D{.x = 0, .y = 0},
			.extent = vulkan::to<vk::Extent2D>(frame.swapchain.extent)
		};

		const auto hdr_attachment_info = vk::RenderingAttachmentInfo{
			.imageView = frame.render_resource.attachments->hdr->attachment.view,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f)
		};

		const auto rendering_info =
			vk::RenderingInfo{.renderArea = rendering_area, .layerCount = 1}
				.setColorAttachments(hdr_attachment_info);

		frame.command_buffer.beginRendering(rendering_info);
		{
			pipeline.direct_lighting.render(frame.command_buffer, frame.resource_set.direct_lighting);
		}
		frame.command_buffer.endRendering();

		const auto post_lighting_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask = vk::AccessFlagBits2::eNone,
			.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
			.image = frame.render_resource.attachments->hdr->attachment.image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		};

		frame.command_buffer.pipelineBarrier2(
			vk::DependencyInfo().setImageMemoryBarriers(post_lighting_barrier)
		);
	}

	void RenderPage::render_post_processing(const Frame& frame) const noexcept
	{
		pipeline.auto_exposure.compute(frame.command_buffer, frame.resource_set.auto_exposure);
	}

	std::expected<void, Error> RenderPage::render_composite(const Frame& frame) noexcept
	{
		const auto pre_composite_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask = vk::AccessFlagBits2::eNone,
			.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.image = frame.swapchain.attachment.image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		};

		frame.command_buffer.pipelineBarrier2(
			vk::DependencyInfo().setImageMemoryBarriers(pre_composite_barrier)
		);

		const auto rendering_area = vk::Rect2D{
			.offset = vk::Offset2D{.x = 0, .y = 0},
			.extent = vulkan::to<vk::Extent2D>(frame.swapchain.extent)
		};

		const auto swapchain_attachment = vk::RenderingAttachmentInfo{
			.imageView = frame.swapchain.attachment.view,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f)
		};

		const auto rendering_info =
			vk::RenderingInfo{.renderArea = rendering_area, .layerCount = 1}
				.setColorAttachments(swapchain_attachment);

		frame.command_buffer.beginRendering(rendering_info);

		pipeline.composite.render(frame.command_buffer, frame.resource_set.composite);

		if (const auto draw_result = context->imgui.draw(frame.command_buffer); !draw_result)
		{
			frame.command_buffer.endRendering();
			return draw_result.error().forward("Draw ImGui failed");
		}

		frame.command_buffer.endRendering();

		const auto post_composite_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eBottomOfPipe,
			.dstAccessMask = vk::AccessFlagBits2::eNone,
			.oldLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.newLayout = vk::ImageLayout::ePresentSrcKHR,
			.image = frame.swapchain.attachment.image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		};

		frame.command_buffer.pipelineBarrier2(
			vk::DependencyInfo().setImageMemoryBarriers(post_composite_barrier)
		);

		return {};
	}

	std::expected<void, Error> RenderPage::draw_frame(const Frame& frame) noexcept
	{
		if (const auto result = frame.command_buffer.begin({}); !result) return Error::from(result);

		frame.render_resource.upload(frame.command_buffer);

		render_objects(frame);
		render_lighting(frame);
		render_post_processing(frame);

		if (const auto composite_result = render_composite(frame); !composite_result)
			return composite_result.error().forward("Render final composite failed");

		if (const auto result = frame.command_buffer.end(); !result) return Error::from(result);

		return {};
	}

	std::expected<void, Error> RenderPage::present_frame(const Frame& frame) noexcept
	{
		const auto command_buffer_submit_info = vk::CommandBufferSubmitInfo{
			.commandBuffer = frame.command_buffer,
		};

		const auto wait_semaphore_info = vk::SemaphoreSubmitInfo{
			.semaphore = frame.sync_primitive.image_available_semaphore,
			.stageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput
		};

		const auto signal_semaphore_info = vk::SemaphoreSubmitInfo{
			.semaphore = frame.render_complete_semaphore,
		};

		const auto submit_info =
			vk::SubmitInfo2()
				.setCommandBufferInfos(command_buffer_submit_info)
				.setWaitSemaphoreInfos(wait_semaphore_info)
				.setSignalSemaphoreInfos(signal_semaphore_info);

		if (const auto result = context->device->resetFences(*frame.sync_primitive.draw_fence); !result)
			return Error::from(result);

		if (const auto result =
				context->device.get().queue.submit2(submit_info, frame.sync_primitive.draw_fence);
			!result)
			return Error::from(result);

		const auto present_result =
			context->swapchain.present(context->device, frame.swapchain, frame.render_complete_semaphore);
		if (!present_result) return present_result.error().forward("Present frame failed");

		return {};
	}
}

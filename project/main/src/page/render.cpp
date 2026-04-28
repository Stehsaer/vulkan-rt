#include "page/render.hpp"
#include "common/util/construct.hpp"
#include "config.hpp"
#include "render/interface/primitive-drawcall.hpp"
#include "render/model/material.hpp"
#include "render/pipeline/forward.hpp"
#include "render/util/per-render-state.hpp"
#include "resource/render-resource.hpp"
#include "resource/sync-primitive.hpp"
#include "vulkan/util/constants.hpp"
#include "vulkan/util/glm.hpp"

#include <SDL3/SDL_events.h>
#include <expected>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace page
{
	std::expected<RenderPage, Error> RenderPage::create(
		resource::Context context,
		render::Model model,
		render::MaterialLayout material_layout
	) noexcept
	{
		auto command_pool_result =
			context.device
				->createCommandPool({
					.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
					.queueFamilyIndex = context.device.get().family,
				})
				.transform_error(Error::from<vk::Result>());
		if (!command_pool_result) return command_pool_result.error().forward("Create command pool failed");
		auto command_pool = std::move(*command_pool_result);

		auto command_buffers_result =
			context.device
				->allocateCommandBuffers({
					.commandPool = command_pool,
					.commandBufferCount = config::INFLIGHT_FRAMES,
				})
				.transform_error(Error::from<vk::Result>());
		if (!command_buffers_result)
			return command_buffers_result.error().forward("Allocate command buffers failed");
		auto command_buffers = std::move(*command_buffers_result);

		auto render_buffers_result =
			std::views::repeat(resource::RenderResource::create, config::INFLIGHT_FRAMES)
			| std::views::transform([&context](auto f) { return f(context.device.get()); })
			| Error::collect();
		if (!render_buffers_result)
			return render_buffers_result.error().forward("Create render buffers failed");
		auto render_buffers = std::move(*render_buffers_result);

		auto sync_primitives_result =
			std::views::repeat(resource::SyncPrimitive::create, config::INFLIGHT_FRAMES)
			| std::views::transform([&context](auto f) { return f(context.device.get()); })
			| Error::collect();
		if (!sync_primitives_result)
			return sync_primitives_result.error().forward("Create sync primitives failed");
		auto sync_primitives = std::move(*sync_primitives_result);

		auto pipeline_result = resource::Pipeline::create(
			context.device.get(),
			material_layout,
			context.swapchain->surface_format.format
		);
		if (!pipeline_result) return pipeline_result.error().forward("Create pipelines failed");
		auto pipeline = std::move(*pipeline_result);

		auto resource_sets_result =
			pipeline.create_resource_sets(context.device.get(), config::INFLIGHT_FRAMES);
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

		auto aux_resource_result = resource::AuxResource::create(context.device.get());
		if (!aux_resource_result)
			return aux_resource_result.error().forward("Create auxiliary resources failed");
		auto aux_resource = std::move(*aux_resource_result);

		return RenderPage(
			std::move(context),
			std::move(command_pool),
			std::move(material_layout),
			std::move(model),
			std::move(pipeline),
			std::move(frame_resources),
			std::move(aux_resource)
		);
	}

	std::expected<RenderPage::ResultType, Error> RenderPage::run_frame() noexcept
	{
		const auto event = handle_events();
		if (event == Event::Quit)
		{
			context.device->waitIdle();
			return ResultType::from<Result::Quit>();
		}

		auto prepare_frame_result = prepare_frame();
		if (!prepare_frame_result)
		{
			context.device->waitIdle();
			return prepare_frame_result.error().forward("Prepare frame failed");
		}
		auto frame = *prepare_frame_result;

		if (const auto draw_result = draw_frame(frame); !draw_result)
		{
			context.device->waitIdle();
			return draw_result.error().forward("Draw frame failed");
		}

		if (const auto present_result = present_frame(frame); !present_result)
		{
			context.device->waitIdle();
			return present_result.error().forward("Present frame failed");
		}

		return ResultType::from<Result::Continue>();
	}

	std::expected<RenderPage::FrameAcquireResult, Error> RenderPage::acquire_frame() noexcept
	{
		/* Cycle & Wait */

		frame_resources.cycle();

		const auto wait_result =
			context.device
				->waitForFences(*frame_resources.current().sync_primitive.draw_fence, vk::True, UINT64_MAX);
		if (wait_result != vk::Result::eSuccess)
			return Error::from(wait_result).forward("Wait for fence failed");

		auto& curr_resource = frame_resources.current();
		auto& prev_resource = frame_resources.prev();

		/* Acquire Swapchain */

		auto swapchain_result = context.swapchain.acquire_next(
			context.instance,
			context.device,
			curr_resource.sync_primitive.image_available_semaphore
		);
		if (!swapchain_result) return swapchain_result.error().forward("Acquire next swapchain image failed");
		auto swapchain_frame = *swapchain_result;

		/* Check, recreate if needed */

		bool waited_idle = false;
		const auto wait_idle = [this, &waited_idle] {
			if (waited_idle) return;
			context.device->waitIdle();
			waited_idle = true;
		};

		if (swapchain_frame.extent_changed || !curr_resource.render_resource.is_complete())
		{
			wait_idle();

			for (auto& resource : frame_resources.iterate())
			{
				auto render_target_result =
					resource.render_resource.resize(context.device.get(), swapchain_frame.extent);
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

	std::expected<RenderPage::SceneData, Error> RenderPage::prepare_scene(glm::u32vec2 extent) noexcept
	{
		auto [drawcalls, transforms] = drawcall_generator.compute(model, glm::mat4(1.0f));
		const auto camera = param.camera.get(extent);
		const auto primary_light = param.primary_light.get();
		const auto exposure_param = param.exposure.get(ImGui::GetIO().DeltaTime, extent);

		return RenderPage::SceneData{
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

	RenderPage::Event RenderPage::handle_events() const noexcept
	{
		SDL_Event event;

		while (SDL_PollEvent(&event))
		{
			ImGui_ImplSDL3_ProcessEvent(&event);

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

	std::expected<RenderPage::Frame, Error> RenderPage::prepare_frame() noexcept
	{
		auto acquire_result = acquire_frame();
		if (!acquire_result) return acquire_result.error().forward("Acquire frame failed");
		auto acquire_frame = *acquire_result;

		/* Check & Recreate */

		/* UI & Scene */

		if (const auto new_frame_result = context.imgui.new_frame(); !new_frame_result)
			return new_frame_result.error().forward("Start new ImGui frame failed");

		ui(acquire_frame.swapchain_frame.extent);

		if (const auto render_result = context.imgui.render(); !render_result)
			return render_result.error().forward("Render ImGui frame failed");

		auto scene_data_result = prepare_scene(acquire_frame.swapchain_frame.extent);
		if (!scene_data_result) return scene_data_result.error().forward("Prepare scene data failed");
		auto scene_data = *scene_data_result;

		/* Update & Bind */

		if (const auto buffer_update_result =
				acquire_frame.curr_resource.render_resource.update(context.device.get(), scene_data);
			!buffer_update_result)
			return buffer_update_result.error().forward("Update render buffer failed");

		acquire_frame.curr_resource.resource_set.update(
			context.device.get(),
			model,
			acquire_frame.curr_resource.render_resource,
			acquire_frame.prev_resource.render_resource,
			aux_resource,
			acquire_frame.swapchain_frame
		);

		return Frame{
			.command_buffer = acquire_frame.curr_resource.command_buffer,
			.render_resource = acquire_frame.curr_resource.render_resource,
			.prev_render_resource = acquire_frame.prev_resource.render_resource,
			.resource_set = acquire_frame.curr_resource.resource_set,
			.sync_primitive = acquire_frame.curr_resource.sync_primitive,
			.swapchain = acquire_frame.swapchain_frame
		};
	}

	std::expected<void, Error> RenderPage::render_final_composite(const Frame& frame) noexcept
	{
		const auto pre_composite_barrier = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.srcAccessMask = vk::AccessFlagBits2::eNone,
			.dstStageMask = vk::PipelineStageFlagBits2::eColorAttachmentOutput,
			.dstAccessMask = vk::AccessFlagBits2::eColorAttachmentWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.image = frame.swapchain.image,
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
			.imageView = frame.swapchain.image_view,
			.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
			.loadOp = vk::AttachmentLoadOp::eClear,
			.storeOp = vk::AttachmentStoreOp::eStore,
			.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f)
		};

		const auto rendering_info =
			vk::RenderingInfo{.renderArea = rendering_area, .layerCount = 1}
				.setColorAttachments(swapchain_attachment);

		frame.command_buffer.beginRendering(rendering_info);

		pipeline.composite_pipeline.render(frame.command_buffer, frame.resource_set.composite_resource_set);

		if (const auto draw_result = context.imgui.draw(frame.command_buffer); !draw_result)
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
			.image = frame.swapchain.image,
			.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
		};

		frame.command_buffer.pipelineBarrier2(
			vk::DependencyInfo().setImageMemoryBarriers(post_composite_barrier)
		);

		return {};
	}

	std::expected<void, Error> RenderPage::draw_frame(const Frame& frame) noexcept
	{
		frame.command_buffer.begin({});

		frame.render_resource.upload(frame.command_buffer);

		pipeline.indirect_pipeline.compute(frame.command_buffer, frame.resource_set.indirect_resource_set);
		pipeline.forward_pipeline.render(frame.command_buffer, frame.resource_set.forward_resource_set);
		pipeline.auto_exposure_pipeline
			.compute(frame.command_buffer, frame.resource_set.auto_exposure_resource_set);

		if (const auto composite_result = render_final_composite(frame); !composite_result)
			return composite_result.error().forward("Render final composite failed");

		frame.command_buffer.end();

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
			.semaphore = frame.sync_primitive.render_finished_semaphore,
		};

		const auto submit_info =
			vk::SubmitInfo2()
				.setCommandBufferInfos(command_buffer_submit_info)
				.setWaitSemaphoreInfos(wait_semaphore_info)
				.setSignalSemaphoreInfos(signal_semaphore_info);

		context.device->resetFences(*frame.sync_primitive.draw_fence);
		context.device.get().queue.submit2(submit_info, frame.sync_primitive.draw_fence);

		const auto present_result =
			context.swapchain
				.present(context.device, frame.swapchain, frame.sync_primitive.render_finished_semaphore);
		if (!present_result) return present_result.error().forward("Present frame failed");

		return {};
	}
}

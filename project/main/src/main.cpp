#include <SDL3/SDL_events.h>
#include <common/formatter/vec.hpp>
#include <common/formatter/vulkan.hpp>
#include <common/utility/error.hpp>

#include <iostream>
#include <optional>
#include <print>
#include <ranges>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>

#include "shader/first_shader.hpp"
#include "vulkan-util/cycle.hpp"
#include "vulkan-util/image-barrier.hpp"
#include "vulkan-util/linked-struct.hpp"
#include "vulkan-util/shader.hpp"
#include "vulkan-window.hpp"

static std::expected<vk::raii::Pipeline, Error> create_graphics_pipeline(const VulkanWindow& window) noexcept
{
	auto shader_module_expected = vkutil::create_shader(window.device, shader::first_shader);
	if (!shader_module_expected) return shader_module_expected.error().forward("Create shader module failed");
	auto shader_module = std::move(*shader_module_expected);

	const auto vertex_stage_info = vk::PipelineShaderStageCreateInfo{
		.stage = vk::ShaderStageFlagBits::eVertex,
		.module = *shader_module,
		.pName = "main_vertex"
	};
	const auto fragment_stage_info = vk::PipelineShaderStageCreateInfo{
		.stage = vk::ShaderStageFlagBits::eFragment,
		.module = *shader_module,
		.pName = "main_fragment"
	};
	const auto shader_stages = std::to_array({vertex_stage_info, fragment_stage_info});

	const auto dynamic_states = std::to_array({vk::DynamicState::eViewport, vk::DynamicState::eScissor});
	const auto dynamic_state_info = vk::PipelineDynamicStateCreateInfo().setDynamicStates(dynamic_states);

	const auto input_assembly_info = vk::PipelineInputAssemblyStateCreateInfo{
		.topology = vk::PrimitiveTopology::eTriangleList,
		.primitiveRestartEnable = vk::False
	};

	const auto vertex_input_info = vk::PipelineVertexInputStateCreateInfo{};

	const auto viewport_info = vk::PipelineViewportStateCreateInfo{
		.viewportCount = 1,
		.pViewports = nullptr,
		.scissorCount = 1,
		.pScissors = nullptr,
	};

	const auto rasterization_info =
		vk::PipelineRasterizationStateCreateInfo{.cullMode = vk::CullModeFlagBits::eNone, .lineWidth = 1.0};
	const auto multisample_info =
		vk::PipelineMultisampleStateCreateInfo{.rasterizationSamples = vk::SampleCountFlagBits::e1};

	const auto swapchain_attachment_blend_state = vk::PipelineColorBlendAttachmentState{
		.blendEnable = vk::False,
		.colorWriteMask = vk::ColorComponentFlagBits::eR
			| vk::ColorComponentFlagBits::eG
			| vk::ColorComponentFlagBits::eB
			| vk::ColorComponentFlagBits::eA
	};
	const auto color_attachment_blend_states = std::to_array({swapchain_attachment_blend_state});
	const auto color_blend_info =
		vk::PipelineColorBlendStateCreateInfo{}.setAttachments(color_attachment_blend_states);

	const auto attachment_formats = std::to_array({window.swapchain_layout.surface_format.format});
	const auto pipeline_rendering_info =
		vk::PipelineRenderingCreateInfo().setColorAttachmentFormats(attachment_formats);

	const auto pipelineLayoutInfo = vk::PipelineLayoutCreateInfo{};

	auto pipeline_layout_expected = window.device.createPipelineLayout(pipelineLayoutInfo);
	if (!pipeline_layout_expected) return Error("Create pipeline layout failed");
	auto pipeline_layout = std::move(*pipeline_layout_expected);

	vkutil::LinkedStruct<vk::GraphicsPipelineCreateInfo> graphics_create_info =
		vk::GraphicsPipelineCreateInfo{
			.pNext = &pipeline_rendering_info,
			.pVertexInputState = &vertex_input_info,
			.pInputAssemblyState = &input_assembly_info,
			.pViewportState = &viewport_info,
			.pRasterizationState = &rasterization_info,
			.pMultisampleState = &multisample_info,
			.pColorBlendState = &color_blend_info,
			.pDynamicState = &dynamic_state_info,
			.layout = *pipeline_layout
		}
			.setStages(shader_stages);
	graphics_create_info.push(
		vk::PipelineRenderingCreateInfo{}.setColorAttachmentFormats(attachment_formats)
	);

	auto pipeline_expected = window.device.createGraphicsPipeline(nullptr, graphics_create_info.get());
	if (!pipeline_expected) return Error("Create graphics pipeline failed");
	return std::move(*pipeline_expected);
}

struct FrameObjects
{
	vk::raii::CommandBuffer command_buffer;

	vk::raii::Fence draw_fence;
	vk::raii::Semaphore render_finished_semaphore;
	vk::raii::Semaphore image_available_semaphore;

	static std::expected<FrameObjects, Error> create(
		const VulkanWindow& vulkan,
		vk::raii::CommandBuffer command_buffer
	) noexcept
	{
		auto fence_expected = vulkan.device.createFence({.flags = vk::FenceCreateFlagBits::eSignaled});
		if (!fence_expected) return Error("Create draw fence failed");

		auto render_finished_semaphore_expected = vulkan.device.createSemaphore({});
		if (!render_finished_semaphore_expected) return Error("Create render finished semaphore failed");

		auto present_complete_semaphore_expected = vulkan.device.createSemaphore({});
		if (!present_complete_semaphore_expected) return Error("Create present complete semaphore failed");

		return FrameObjects{
			.command_buffer = std::move(command_buffer),
			.draw_fence = std::move(*fence_expected),
			.render_finished_semaphore = std::move(*render_finished_semaphore_expected),
			.image_available_semaphore = std::move(*present_complete_semaphore_expected)
		};
	}
};

int main()
{
	try
	{
		/* Context */

		const auto create_info = VulkanWindow::CreateInfo{
			.window_info = {.title = "Vulkan Gltf RT", .initial_size = {800, 600}},
			.app_info =
				{.application_name = "Vulkan Gltf RT", .application_version = VK_MAKE_VERSION(0, 1, 0)},
			.features = {.validation = true},
		};

		auto vulkan_expected = VulkanWindow::create(create_info);
		if (!vulkan_expected) vulkan_expected.error().throw_self("Create context failed");
		auto vulkan = std::move(*vulkan_expected);

		/* Pipeline */

		auto pipeline_expected = create_graphics_pipeline(vulkan);
		if (!pipeline_expected) pipeline_expected.error().throw_self("Create graphics pipeline failed");
		auto graphics_pipeline = std::move(*pipeline_expected);

		/* Command Pool */

		auto command_pool_expected = vulkan.device.createCommandPool(
			{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
			 .queueFamilyIndex = vulkan.queues.graphics_index}
		);
		if (!command_pool_expected)
			Error(command_pool_expected.error(), "Create command pool failed").throw_self();
		auto command_pool = std::move(*command_pool_expected);

		/* Command Buffer */

		auto command_buffers_expected = vulkan.device.allocateCommandBuffers({
			.commandPool = *command_pool,
			.level = vk::CommandBufferLevel::ePrimary,
			.commandBufferCount = 3,
		});
		if (!command_buffers_expected)
			Error(command_buffers_expected.error(), "Allocate command buffers failed").throw_self();
		auto command_buffers = std::move(*command_buffers_expected);

		/* Render Objects */

		auto object_list_expected =
			command_buffers
			| std::views::transform([&vulkan](auto& command_buffer) {
				  return FrameObjects::create(vulkan, std::move(command_buffer));
			  })
			| std::ranges::to<std::vector>()
			| Error::collect_vec("Create frame objects failed");
		if (!object_list_expected) object_list_expected.error().throw_self();
		auto object_cycle = vkutil::Cycle<FrameObjects>::create(std::move(*object_list_expected));

		bool quit = false;
		while (!quit)
		{
			SDL_Event event;
			while (SDL_PollEvent(&event))
			{
				switch (event.type)
				{
				case SDL_EVENT_QUIT:
					quit = true;
					break;
				case SDL_EVENT_WINDOW_MINIMIZED:
					continue;
				default:
					break;
				}
			}

			vulkan.queues.graphics->waitIdle();

			object_cycle.cycle();
			const auto& current_frame_object = object_cycle.current();

			/* Wait For Fence */

			if (const auto wait_result = vulkan.device.waitForFences(
					{*current_frame_object.draw_fence},
					vk::True,
					std::numeric_limits<uint64_t>::max()
				);
				wait_result != vk::Result::eSuccess)
				Error(wait_result, "Wait for draw fence failed").throw_self();
			const auto& command_buffer = current_frame_object.command_buffer;

			/* Acquire Swapchain */

			const auto acquire_expected =
				vulkan.acquire_swapchain_image(object_cycle.prev().image_available_semaphore);
			if (!acquire_expected) acquire_expected.error().throw_self("Acquire swapchain image failed");
			const auto& [extent, index, swapchain_image] = *acquire_expected;

			/* Actual Rendering */

			command_buffer.begin({.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
			{
				const auto acquire_image_barriers =
					std::to_array({vkutil::image_barrier::swapchain_acquire(swapchain_image.image)});
				command_buffer.pipelineBarrier2(
					vk::DependencyInfo{}.setImageMemoryBarriers(acquire_image_barriers)
				);

				const auto viewport = vk::Viewport{
					.x = 0.0f,
					.y = 0.0f,
					.width = static_cast<float>(extent.x),
					.height = static_cast<float>(extent.y),
					.minDepth = 0.0f,
					.maxDepth = 1.0f,
				};
				const auto scissor = vk::Rect2D{
					.offset = vk::Offset2D{.x = 0,            .y = 0            },
					.extent = vk::Extent2D{.width = extent.x, .height = extent.y},
				};

				const auto swapchain_attachment_info = vk::RenderingAttachmentInfo{
					.imageView = swapchain_image.view,
					.imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
					.loadOp = vk::AttachmentLoadOp::eClear,
					.storeOp = vk::AttachmentStoreOp::eStore,
					.clearValue = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f),
				};

				const auto attachment_info_list = std::to_array({swapchain_attachment_info});
				const auto rendering_info =
					vk::RenderingInfo{
						.renderArea = scissor,
						.layerCount = 1,
					}
						.setColorAttachments(attachment_info_list);

				command_buffer.beginRendering(rendering_info);
				{
					command_buffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphics_pipeline);
					command_buffer.setViewport(0, {viewport});
					command_buffer.setScissor(0, {scissor});
					command_buffer.draw(3, 1, 0, 0);
				}
				command_buffer.endRendering();

				const auto present_image_barriers =
					std::to_array({vkutil::image_barrier::swapchain_present(swapchain_image.image)});
				command_buffer.pipelineBarrier2(
					vk::DependencyInfo{}.setImageMemoryBarriers(present_image_barriers)
				);
			}
			command_buffer.end();

			/* Submit */

			const auto graphic_submit_wait_semaphores =
				std::to_array({*object_cycle.prev().image_available_semaphore});
			const auto graphic_submit_signal_semaphores =
				std::to_array({*current_frame_object.render_finished_semaphore});
			const auto graphic_submit_command_buffers = std::to_array({*command_buffer});
			const auto graphic_submit_wait_stages =
				std::to_array<vk::PipelineStageFlags>({vk::PipelineStageFlagBits::eColorAttachmentOutput});
			const auto graphic_submit_info =
				vk::SubmitInfo{}
					.setCommandBuffers(graphic_submit_command_buffers)
					.setWaitSemaphores(graphic_submit_wait_semaphores)
					.setSignalSemaphores(graphic_submit_signal_semaphores)
					.setWaitDstStageMask(graphic_submit_wait_stages);

			vulkan.device.resetFences({*current_frame_object.draw_fence});
			vulkan.queues.graphics->submit(graphic_submit_info, *current_frame_object.draw_fence);

			/* Present */

			const auto present_result =
				vulkan.present_swapchain_image(index, current_frame_object.render_finished_semaphore);
			if (!present_result) present_result.error().throw_self("Present swapchain image failed");
		}

		vulkan.device.waitIdle();

		return 0;
	}
	catch (const Error& error)
	{
		std::println(std::cerr, "Error occurred: {:msg}", error.origin());
		for (const auto& [idx, entry] : std::views::enumerate(error.trace))
			std::println(std::cerr, "[#{}]: {}", idx, entry);

		return 1;
	}
}

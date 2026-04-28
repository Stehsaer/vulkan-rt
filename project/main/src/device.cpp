#include "resource/context.hpp"
#include "vulkan/context/instance.hpp"

namespace resource
{
	std::expected<Context, Error> Context::create() noexcept
	{
		const auto window_config = vulkan::WindowConfig{
			.title = "Vulkan-RT",
			.initial_size = {1280, 720},
			.resizable = true,
			.initial_fullscreen = false
		};
		const auto instance_config = vulkan::InstanceConfig{};
		auto instance_result = vulkan::SurfaceInstanceContext::create(window_config, instance_config);
		if (!instance_result) return instance_result.error().forward("Create instance context failed");
		auto instance = std::move(*instance_result);

		const auto device_option = vulkan::DeviceOption{};
		auto device_result = vulkan::SurfaceDeviceContext::create(instance, device_option);
		if (!device_result) return device_result.error().forward("Create device context failed");
		auto device = std::move(*device_result);

		auto swapchain_result = vulkan::SwapchainContext::create(
			instance,
			device,
			vulkan::SwapchainContext::Config{.format = vulkan::SwapchainContext::Format::LinearUnorm8}
		);
		if (!swapchain_result) return swapchain_result.error().forward("Create swapchain context failed");
		auto swapchain = std::move(*swapchain_result);

		const auto format = swapchain->surface_format.format;
		const auto pipeline_rendering_info =
			vk::PipelineRenderingCreateInfo().setColorAttachmentFormats(format);
		const auto imgui_config = vulkan::ImGuiContext::Config{
			.render_scheme =
				vulkan::ImGuiContext::Config::DynamicRendering{.rendering_info = pipeline_rendering_info}
		};
		auto imgui_result = vulkan::ImGuiContext::create(instance, device.get(), imgui_config);
		if (!imgui_result) return imgui_result.error().forward("Create ImGui context failed");
		auto imgui = std::move(*imgui_result);

		return Context{
			.instance = std::move(instance),
			.device = std::move(device),
			.swapchain = std::move(swapchain),
			.imgui = std::move(imgui)
		};
	}
}

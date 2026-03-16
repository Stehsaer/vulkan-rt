#include "vulkan/context/imgui.hpp"
#include "common/util/error.hpp"

#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	static PFN_vkVoidFunction imgui_load_vk_function(const char* function_name, void* user_data) noexcept
	{
		auto& instance_ref =
			*reinterpret_cast<std::reference_wrapper<const SurfaceInstanceContext>*>(user_data);
		return instance_ref.get()->instance.getProcAddr(function_name);
	}

	std::expected<ImGuiContext, Error> ImGuiContext::create(
		const SurfaceInstanceContext& instance_context,
		const DeviceContext& device_context,
		const Config& config
	) noexcept
	{
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();

		/* Setup IO */

		auto& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

		/* Setup Style */

		const float main_scale = SDL_GetWindowDisplayScale(instance_context->window);
		auto& style = ImGui::GetStyle();

		ImGui::StyleColorsDark();
		style.FontSizeBase = 16.0;
		style.ScaleAllSizes(main_scale);

		io.Fonts->AddFontDefaultVector();
		io.ConfigDpiScaleFonts = true;

		/* Initialize ImGui SDL3 Backend */

		if (!ImGui_ImplSDL3_InitForVulkan(instance_context->window))
		{
			ImGui::DestroyContext();
			return Error("Initialize ImGui SDL3 backend failed");
		}

		/* Initialize ImGui Vulkan Backend */

		if (auto instance_ref = std::cref(instance_context);
			!ImGui_ImplVulkan_LoadFunctions(api_version, imgui_load_vk_function, &instance_ref))
			return Error("Load functions for ImGui vulkan backend failed");

		const auto pipeline_info = std::visit(
			[](const auto& scheme) -> ImGui_ImplVulkan_PipelineInfo {
				using T = std::decay_t<decltype(scheme)>;
				if constexpr (std::derived_from<T, Config::DynamicRendering>)
					return {
						.RenderPass = nullptr,
						.Subpass = 0,
						.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
						.PipelineRenderingCreateInfo = scheme.rendering_info,
						.SwapChainImageUsage = {}
					};
				else
					return {
						.RenderPass = scheme.render_pass,
						.Subpass = scheme.subpass_index,
						.MSAASamples = static_cast<VkSampleCountFlagBits>(scheme.sample_count),
						.PipelineRenderingCreateInfo = {},
						.SwapChainImageUsage = {}
					};
			},
			config.render_scheme
		);

		const auto empty_pipeline_info = ImGui_ImplVulkan_PipelineInfo{
			.RenderPass = nullptr,
			.Subpass = 0,
			.MSAASamples = VK_SAMPLE_COUNT_1_BIT,
			.PipelineRenderingCreateInfo = {},
			.SwapChainImageUsage = {}
		};

		const auto placeholder_custom_shader_module_info = VkShaderModuleCreateInfo{
			.sType = VK_STRUCTURE_TYPE_MAX_ENUM,
			.pNext = nullptr,
			.flags = 0,
			.codeSize = 0,
			.pCode = nullptr
		};

		auto init_info = ImGui_ImplVulkan_InitInfo{
			.ApiVersion = api_version,
			.Instance = *instance_context->instance,
			.PhysicalDevice = *device_context.phy_device,
			.Device = *device_context.device,
			.QueueFamily = device_context.family,
			.Queue = *device_context.queue,
			.DescriptorPool = nullptr,
			.DescriptorPoolSize = IMGUI_IMPL_VULKAN_MINIMUM_IMAGE_SAMPLER_POOL_SIZE,
			.MinImageCount = 2,
			.ImageCount = 3,
			.PipelineCache = nullptr,
			.PipelineInfoMain = pipeline_info,
			.PipelineInfoForViewports = empty_pipeline_info,
			.UseDynamicRendering = std::holds_alternative<Config::DynamicRendering>(config.render_scheme),
			.Allocator = nullptr,
			.CheckVkResultFn = check_imgui_vk_result,
			.MinAllocationSize = 1024 * 1024,
			.CustomShaderVertCreateInfo = placeholder_custom_shader_module_info,
			.CustomShaderFragCreateInfo = placeholder_custom_shader_module_info
		};

		if (!ImGui_ImplVulkan_Init(&init_info))
		{
			ImGui_ImplSDL3_Shutdown();
			ImGui::DestroyContext();

			return Error("Initialize ImGui Vulkan backend failed");
		}

		return ImGuiContext();
	}

	void ImGuiContext::check_imgui_vk_result(VkResult err)
	{
		if (err != VK_SUCCESS) throw Error("ImGui vulkan error", std::format("{}", vk::Result(err)));
	}

	void ImGuiContext::process_event(const SDL_Event& event) noexcept
	{
		ImGui_ImplSDL3_ProcessEvent(&event);
	}

	std::expected<void, Error> ImGuiContext::new_frame() noexcept
	{
		if (state != State::Idle) return Error("ImGui context is in wrong state");

		ImGui_ImplVulkan_NewFrame();
		ImGui_ImplSDL3_NewFrame();
		ImGui::NewFrame();

		state = State::Logic;

		return {};
	}

	std::expected<void, Error> ImGuiContext::render() noexcept
	{
		if (state != State::Logic) return Error("ImGui context is in wrong state");

		ImGui::Render();
		state = State::Complete;

		return {};
	}

	std::expected<void, Error> ImGuiContext::draw(const vk::raii::CommandBuffer& command_buffer) noexcept
	{
		if (state != State::Complete) return Error("ImGui context is in wrong state");

		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), *command_buffer);
		state = State::Idle;

		return {};
	}

	ImGuiContext::ContextDeleter::~ContextDeleter() noexcept
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplSDL3_Shutdown();
		ImGui::DestroyContext();
	}
}

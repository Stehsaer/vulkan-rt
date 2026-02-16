#include "vulkan/context/imgui.hpp"
#include "common/util/error.hpp"
#include "common/util/variant.hpp"

#include <SDL3/SDL_video.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_vulkan.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>

extern "C"
{
	extern const std::byte _binary_proggy_forever_ttf_start[];
	extern const std::byte _binary_proggy_forever_ttf_end[];
}

namespace vulkan
{
	static const auto proggy_forever_ttf =
		std::span(&_binary_proggy_forever_ttf_start[0], &_binary_proggy_forever_ttf_end[0]);

	std::expected<ImGuiContext, Error> ImGuiContext::create(
		const InstanceContext& instance_context,
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
		style.ScaleAllSizes(main_scale);

		std::vector font_data_temp(std::from_range, proggy_forever_ttf);
		ImFontConfig font_config;
		font_config.FontDataOwnedByAtlas = false;

		const auto add_font_result = io.Fonts->AddFontFromMemoryTTF(
			font_data_temp.data(),
			proggy_forever_ttf.size_bytes(),
			0.0f,
			&font_config
		);
		if (add_font_result == nullptr) return Error("Add font failed");

		/* Initialize ImGui SDL3 Backend */

		if (!ImGui_ImplSDL3_InitForVulkan(instance_context->window))
		{
			ImGui::DestroyContext();
			return Error("Initialize ImGui SDL3 backend failed");
		}

		/* Initialize ImGui Vulkan Backend */

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
			.QueueFamily = device_context.graphics_queue.family_index,
			.Queue = **device_context.graphics_queue.queue,
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
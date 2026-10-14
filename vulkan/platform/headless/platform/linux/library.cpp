#include "vulkan/platform/headless/library.hpp"
#include "common/container/error.hpp"

#include <dlfcn.h>
#include <exception>
#include <expected>
#include <memory>
#include <new>
#include <vulkan/vulkan_core.h>

namespace vulkan
{
	struct VulkanLibraryInfo
	{
		void* handle;

		static void deleter(VulkanLibraryInfo* info) noexcept
		{
			if (info == nullptr) return;
			if (info->handle != nullptr) dlclose(info->handle);
			delete info;
		}
	};

	std::expected<VulkanLibrary, Error> load_vulkan_library() noexcept
	{
		void* handle = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);  // NOLINT: false clangd warning
		if (handle == nullptr) return Error("Open vulkan library failed", dlerror());

		try
		{
			auto* library_info = new VulkanLibraryInfo(handle);

			return std::unique_ptr<VulkanLibraryInfo, void (*)(VulkanLibraryInfo*)>(
				library_info,
				VulkanLibraryInfo::deleter
			);
		}
		catch (const std::bad_alloc& err)
		{
			std::terminate();
		}
	}

	std::expected<PFN_vkGetInstanceProcAddr, Error> get_vulkan_dyn_func(
		const VulkanLibraryInfo& info
	) noexcept
	{
		auto* func = dlsym(info.handle, "vkGetInstanceProcAddr");
		if (func == nullptr) return Error("Function 'vkGetInstanceProcAddr' not found");
		return reinterpret_cast<PFN_vkGetInstanceProcAddr>(func);
	}
}

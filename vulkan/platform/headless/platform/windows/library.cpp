#include "vulkan/platform/headless/library.hpp"
#include "common/container/error.hpp"

#include <bit>
#include <exception>
#include <expected>
#include <format>
#include <memory>
#include <new>
#include <string>
#include <vulkan/vulkan_core.h>
#include <windows.h>

/* WARN: AI generated platform-specific code, not reviewed yet */

namespace vulkan
{
	namespace
	{
		[[nodiscard]]
		std::string get_last_error_message() noexcept
		{
			const DWORD code = GetLastError();
			if (code == 0) return "Unknown error";

			char buffer[512] = {};
			const DWORD length = FormatMessageA(
				FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr,
				code,
				MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				buffer,
				static_cast<DWORD>(sizeof(buffer)),
				nullptr
			);
			if (length == 0) return std::format("Win32 error {}", code);

			// FormatMessageA terminates the message with CR/LF, strip it
			std::string message(buffer, length);
			while (!message.empty() && (message.back() == '\r' || message.back() == '\n')) message.pop_back();
			return message;
		}
	}

	struct VulkanLibraryInfo
	{
		HMODULE handle;

		static void deleter(VulkanLibraryInfo* info) noexcept
		{
			if (info == nullptr) return;
			if (info->handle != nullptr) FreeLibrary(info->handle);
			delete info;
		}
	};

	std::expected<VulkanLibrary, Error> load_vulkan_library() noexcept
	{
		HMODULE handle = LoadLibraryA("vulkan-1.dll");
		if (handle == nullptr) return Error("Open vulkan library failed", get_last_error_message());

		try
		{
			auto* library_info = new VulkanLibraryInfo(handle);

			return std::unique_ptr<VulkanLibraryInfo, void (*)(VulkanLibraryInfo*)>(
				library_info,
				VulkanLibraryInfo::deleter
			);
		}
		catch (const std::bad_alloc&)
		{
			std::terminate();
		}
	}

	std::expected<PFN_vkGetInstanceProcAddr, Error> get_vulkan_dyn_func(
		const VulkanLibraryInfo& info
	) noexcept
	{
		FARPROC func = GetProcAddress(info.handle, "vkGetInstanceProcAddr");
		if (func == nullptr)
			return Error("Function 'vkGetInstanceProcAddr' not found", get_last_error_message());

		// Win32 hands back an untyped FARPROC; both are equally sized function pointers
		static_assert(sizeof(PFN_vkGetInstanceProcAddr) == sizeof(FARPROC));
		return std::bit_cast<PFN_vkGetInstanceProcAddr>(func);
	}
}

set_xmakever("3.0.6")
set_project("Vulkan RT")

-- Compile modes
add_rules("mode.debug", "mode.release", "mode.releasedbg", "mode.profile")

-- Compile policies
set_policy("build.warning", true)
set_policy("build.intermediate_directory", false)
set_policy("run.autobuild", true)

-- Compile settings
set_encodings("utf-8")
set_warnings("all", "pedantic", "extra")
set_languages("c++23")
add_vectorexts("sse", "sse2", "avx", "avx2")
add_cxxflags("-Wno-maybe-uninitialized")

-- Linux atomic
if is_plat("linux") then
	add_syslinks("atomic")
end

-- Third-party packages
add_requires(
	-- Utilities
	"gzip-hpp v0.1.0",
	"doctest 2.4.12",
	"argparse v3.2",
	"libcoro v0.16.0",
	"mio 2023.3.3",

	-- Graphics
	"libsdl3",
	"glm 1.0.2",
	"imgui[sdl3,freetype,vulkan_no_proto] v1.92.6-docking",

	-- Vulkan
	"vulkan-headers",
	"vulkan-hpp",
	"vulkan-memory-allocator 3.3.0"
)

add_requireconfs("**vulkan-hpp", {version = "1.4.309", override = true, system = false})
add_requireconfs("**vulkan-headers", {version = "1.4.309+0", override = true, system = false})
add_requireconfs("**libsdl3", {version = "3.4.2", override = true, system = false})

-- Global defines
add_defines(
	"GLM_FORCE_DEPTH_ZERO_TO_ONE", 
	"GLM_ENABLE_EXPERIMENTAL"
)

-- Vulkan specific
add_defines(
	"VK_NO_PROTOTYPES",
	"VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1",
	"VMA_DYNAMIC_VULKAN_FUNCTIONS",
	"VULKAN_HPP_ENABLE_DYNAMIC_LOADER_TOOL=0",
	"VULKAN_HPP_NO_EXCEPTIONS", 
	"VULKAN_HPP_NO_STRUCT_CONSTRUCTORS", 
	"VULKAN_HPP_ASSERT_ON_RESULT=;"
)

-- Rules
includes("rule")

-- Targets
includes("shader")
includes("lib")
includes("vulkan")
includes("project")
includes("model")
includes("render")
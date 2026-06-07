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

if is_arch("x86_64") then
	add_vectorexts("avx2")
end

-- GCC false warning
if is_plat("linux") then
	add_cxxflags("-Wno-maybe-uninitialized")
	add_cxxflags("-Wno-uninitialized")
	add_cxxflags("-Wno-stringop-overflow")
	add_cxxflags("-Wno-array-bounds")
end

-- I hate <windows.h> :-(
-- Stop defining min and max pls...
if is_plat("windows") then
	add_defines("NOMINMAX")
end

-- Linux atomic
if is_plat("linux") then
	add_syslinks("atomic")
end

-- Third-party packages
includes("package/*.lua")
add_requires(
	-- Utilities
	"gzip-hpp v0.1.0",
	"doctest 2.4.12",
	"argparse v3.2",
	"mio 2023.3.3",
	"libassert[magic_enum=n] v2.2.1",
	"nlohmann_json v3.12.0",

	-- Graphics
	"libsdl3",
	"glm 1.0.2",
	"imgui[sdl3,freetype,vulkan_no_proto] v1.92.6-docking",

	-- Vulkan
	"vulkan-headers",
	"vulkan-hpp",
	"vulkan-memory-allocator 3.3.0"
)

add_requireconfs("**vulkan-headers", {version = "v1.4.351", override = true, system = false})
add_requireconfs("**vulkan-hpp", {version = "v1.4.351", override = true, system = false})
add_requireconfs("**libsdl3", {version = "3.4.2", override = true, system = false})

add_requires("libcoro-alt v0.16.0", {alias = "libcoro"})

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
	"VULKAN_HPP_ASSERT_ON_RESULT=(void)",
	"VULKAN_HPP_USE_STD_EXPECTED"
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
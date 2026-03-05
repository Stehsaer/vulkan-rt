set_xmakever("3.0.6")

-- Compile modes
add_rules("mode.debug", "mode.release", "mode.releasedbg")

-- Compile policies
set_policy("build.warning", true)
set_policy("build.intermediate_directory", false)
set_policy("run.autobuild", true)

-- Compile settings
set_encodings("utf-8")
set_warnings("all", "pedantic", "extra")
set_languages("c++23")
add_vectorexts("sse", "sse2", "avx", "avx2")

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

	-- Graphics
	"libsdl3 3.4.0",
	"stb 2025.03.14",
	"glm 1.0.2",
	"meshoptimizer v0.25",
	"rapidobj v1.1",

	-- Vulkan
	"vulkan-headers",
	"vulkan-hpp",
	"vulkan-memory-allocator 3.3.0"
)

-- NOTE: temporary fix for imgui. Remove after upstream xrepo gets the fix
add_repositories("repo repo")
add_requires("imgui-vk-noproto v1.92.6-docking", {configs = {sdl3 = true, freetype = true, vulkan_no_proto = true}, alias = "imgui"})

add_requireconfs("**vulkan-hpp", {version = "1.4.309", override = true, system = false})
add_requireconfs("**vulkan-headers", {version = "1.4.309+0", override = true, system = false})

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
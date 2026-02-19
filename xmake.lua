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
	"vulkan-headers 1.4.309+0",
	"vulkan-hpp 1.4.309",
	"vulkan-memory-allocator 3.3.0"
)

add_requires("imgui v1.92.5-docking", {configs = {sdl3 = true, vulkan = true, freetype = true}})

-- Global defines
add_defines(
	"GLM_FORCE_DEPTH_ZERO_TO_ONE", 
	"GLM_ENABLE_EXPERIMENTAL"
)
add_defines("TINYGLTF_NOEXCEPTION")
add_defines(
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
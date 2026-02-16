-- Convenient SDL window and vulkan context creator

target("vulkan.context")
	set_kind("static")

	add_includedirs("include", {public = true})
	add_includedirs("internal", {public = false})
	add_headerfiles("include/(**.hpp)")
	add_files("src/**.cpp")
	
	add_files("asset/*", {rules = "utils.bin2obj"})

	add_packages(
		"vulkan-hpp",
		"libsdl3",
		"glm",
		"imgui",
		{public=true}
	)
	add_deps("lib.common", "vulkan.util", "vulkan.alloc", {public=true})
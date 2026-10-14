target("vulkan.platform.windowed")
	set_kind("static")

	add_files("src/**.cpp")
	add_headerfiles("include/(**.hpp)")
	add_includedirs("include", {public = true})

	add_deps("vulkan.platform.common")
	add_deps("vulkan.common", {public = true})
	
	add_packages(
		"vulkan-hpp",
		"libsdl3",
		"glm",
		"imgui",
		{public = true}
	)
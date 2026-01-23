target("vulkan-util")
	set_kind("static")

	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")
	add_files("src/**.cpp")

	add_packages(
		"vulkan-hpp", 
		"libsdl3", 
		"vulkan-memory-allocator-hpp", 
		{public=true}
	)
	add_deps("common", "vma-hpp", {public=true})
target("vulkan.util")
	set_kind("static")

	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")
	add_files("src/**.cpp")

	add_packages("glm", "vulkan-hpp", {public = true})
	add_deps(
		"lib.common",
		"lib.image",
		"vulkan.alloc",
		"vulkan.interface",
		"vulkan.numeric",
		{public = true}
	)
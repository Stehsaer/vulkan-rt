target("vulkan.common")
	set_kind("static")

	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")
	add_files("src/**.cpp")

	add_packages("vulkan-hpp", {public = true})
	add_deps("common", "image", "vulkan.alloc", {public = true})
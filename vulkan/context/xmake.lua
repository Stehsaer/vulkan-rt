target("vulkan.context")
	set_kind("static")

	add_includedirs("include", {public = true})
	add_includedirs("impl", {public = false})
	add_headerfiles("include/(**.hpp)")
	add_files("src/**.cpp")

	add_packages("vulkan-hpp", "libsdl3", "glm", {public=true})
	add_deps("lib.common", "vulkan.util", "vulkan.alloc", {public=true})
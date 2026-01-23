target("vulkan-context")
	set_kind("static")

	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")
	add_files("src/**.cpp")

	add_packages("vulkan-hpp", "libsdl3", "glm", {public=true})
	add_deps("common", "vulkan-util", "vma-hpp", {public=true})
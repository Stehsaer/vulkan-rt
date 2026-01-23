target("vma-hpp")
	set_kind("static")

	add_files("src/**.cpp")
	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")

	add_packages("vulkan-memory-allocator", "vulkan-hpp", {public = true})
	add_deps("common", {public=true})
	
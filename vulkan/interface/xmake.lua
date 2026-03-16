target("vulkan.interface")
	set_kind("headeronly")

	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")

	add_packages("vulkan-hpp", {public = true})
	add_deps("lib.common", "lib.image", "vulkan.alloc", {public = true})
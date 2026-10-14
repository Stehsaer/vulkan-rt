target("vulkan.platform.common")
	set_kind("static")

	add_files("src/**.cpp")
	add_headerfiles("include/(**.hpp)")
	add_includedirs("include", {public = true})
	
	add_packages("vulkan-hpp", {public = true})
	add_deps("vulkan.common", {public = true})
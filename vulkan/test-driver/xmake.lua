-- Test helper library for GPU tests
target("vulkan.test-driver")
	set_kind("static")
	set_default(false)

	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")
	add_files("src/**.cpp")

	add_deps("vulkan.context", {public = true})
	add_packages("doctest", {public = true})
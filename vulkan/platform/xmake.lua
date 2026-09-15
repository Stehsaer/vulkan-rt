-- Convenient SDL window and vulkan context creator

target("vulkan.platform")
	set_kind("static")

	add_includedirs("include", {public = true})
	add_includedirs("internal", {public = false})
	add_headerfiles("include/(**.hpp)")
	add_files("src/**.cpp")
	
	add_packages(
		"vulkan-hpp",
		"libsdl3",
		"glm",
		"imgui",
		{public = true}
	)
	add_deps(
		"common",
		"vulkan.common",
		"vulkan.alloc",
		"vulkan.container",
		{public = true}
	)

target("vulkan.platform.test")
	set_kind("binary")
	set_default(false)

	add_deps("vulkan.platform")
	add_packages("doctest")
	add_defines("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

	for _, testfile in ipairs(os.files("test/*.cpp")) do
		 add_tests(path.basename(testfile), {files = testfile})
	end
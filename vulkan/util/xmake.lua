-- Convenient vulkan utilities, reducing boilerplate

target("vulkan.util")
	set_kind("static")

	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")
	add_files("src/**.cpp")

	add_packages("glm", {public = true})
	add_deps("lib.common", "vulkan.alloc", "vulkan.loader", {public = true})

	for _, testfile in ipairs(os.files("test/*.cpp")) do
         add_tests(path.basename(testfile), {
             kind = "binary",
             files = {testfile},
             packages = "doctest",
             defines = "DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN"
		})
    end
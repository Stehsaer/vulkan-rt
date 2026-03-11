-- Convenient vulkan utilities, reducing boilerplate

target("vulkan.util")
	set_kind("static")

	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")
	add_files("src/**.cpp")

	add_packages("glm", "vulkan-hpp", {public = true})
	add_deps("lib.common", "lib.image", "vulkan.alloc", {public = true})

target("vulkan.util.cpu-test")
	set_kind("binary")
	set_default(false)

	add_packages("doctest")
	add_deps("vulkan.util")
	add_defines("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

	for _, testfile in ipairs(os.files("test/*.cpp")) do
         add_tests(path.basename(testfile), {files = {testfile}})
    end
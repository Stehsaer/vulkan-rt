-- Common utilities library, including formatting and error classes.

target("common")
	set_kind("static")
	add_files("src/**.cpp")	

	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")
	add_packages("cppcodec", "nlohmann_json")
	add_packages("libassert", "vulkan-hpp", {public = true})

target("common.test")
	set_kind("binary")
	set_default(false)

	add_deps("common")
	add_packages("doctest", "vulkan-hpp")

	set_runenv("TEMP_DIR", os.tmpdir())
	add_files("test/*.cpp")
	set_group("test")
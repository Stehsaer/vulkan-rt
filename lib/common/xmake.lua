-- Common utilities library, including formatting and error classes.

target("lib.common")
	if #os.files("src/**.cpp") > 0 then
		set_kind("static")
		add_files("src/**.cpp")
	else
		set_kind("headeronly")
	end

	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")
	add_packages("vulkan-hpp", {public = true})

target("lib.common.test")
	set_kind("binary")
	set_default(false)

	add_deps("lib.common")
	add_packages("doctest")
	add_defines("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

	set_runenv("TEMP_DIR", os.tmpdir())

	for _, testfile in ipairs(os.files("test/*.cpp")) do
		add_tests(path.basename(testfile), {files = {testfile}})
    end
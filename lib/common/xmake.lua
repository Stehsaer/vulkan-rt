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
	add_packages("vulkan-hpp", {public=true})

	for _, testfile in ipairs(os.files("test/*.cpp")) do
         add_tests(path.basename(testfile), {
             kind = "binary",
             files = {testfile},
             packages = "doctest",
             defines = "DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN"
		})
    end
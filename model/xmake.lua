add_requires("meshoptimizer v0.25")

-- 3D model interface
target("model")
	set_kind("static")

	add_files("src/*.cpp")
	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")

	add_deps("common", "image")
	add_packages("meshoptimizer", "mio")
target_end()

target("model.test")
	set_kind("binary")
	set_default(false)

	add_deps("model")
	add_packages("doctest")
	add_defines("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

	for _, testfile in ipairs(os.files("test/*.cpp")) do
		add_tests(path.basename(testfile), {files = {testfile}})
	end
target_end()

includes("*")
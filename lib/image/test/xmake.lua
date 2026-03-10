-- Asset for unit test
target("lib.image.test.asset")
	set_kind("static")
	set_default(false)

	add_includedirs("asset-api/include", {public = true})
	add_files("asset-api/src/*.cpp")
	add_files("asset/*", {rules = "utils.bin2obj"})

-- Unit test
target("lib.image.cpu-test")
	set_kind("binary")
	set_default(false)

	add_defines("DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN")

	add_packages("doctest")
	add_deps("lib.image", "lib.image.test.asset")

	for _, testfile in ipairs(os.files("cpu-case/*.cpp")) do
		add_tests(path.basename(testfile), {files = testfile})
	end

-- Unit tests running on GPU
target("lib.image.gpu-test")
	set_kind("binary")
	set_default(false)

	add_packages("doctest")
	add_deps("lib.image", "lib.image.test.asset", "vulkan.context")

	for _, testfile in ipairs(os.files("gpu-case/*.cpp")) do
		add_tests(path.basename(testfile), {
			files = testfile,
			runenvs = {
				["TEMP_DIR"] = os.tmpdir()
			}
		})
	end
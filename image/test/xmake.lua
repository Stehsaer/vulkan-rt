-- Asset for unit test
target("image.test.asset")
	set_kind("static")
	set_default(false)

	add_includedirs("asset-api/include", {public = true})
	add_files("asset-api/src/*.cpp")
	add_files("asset/*", {rules = "utils.bin2obj"})

-- Unit test
target("image.cpu-test")
	set_kind("binary")
	set_default(false)

	add_packages("doctest")
	add_deps("image", "image.test.asset")
	add_files("cpu-case/*.cpp")
	add_files("doctest-impl.cpp")
	set_group("test")

-- Unit tests running on GPU
target("image.gpu-test")
	set_kind("binary")
	set_default(false)

	add_packages("doctest")
	add_deps("image", "image.test.asset", "vulkan.platform.headless")
	set_runenv("TEMP_DIR", os.tmpdir())
	add_files("gpu-case/*.cpp")
	set_group("test")
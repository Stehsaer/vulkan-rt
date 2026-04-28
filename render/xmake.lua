slang("render")
	add_moduledirs("shader/lib")

target("render")
	set_kind("static")
	add_rules("compile.slang")

	add_files("src/**.cpp")
	add_files("shader/program/**.slang", {base_dir = path.absolute("shader/program")})
	add_includedirs("include", {public = true})
	add_includedirs("impl", {public = false})
	add_headerfiles("include/(**.hpp)")

	add_slang_deps("common", "render")
	add_deps("lib.model", "vulkan.alloc", "vulkan.util", "lib.scene", {public = true})
	add_packages("libcoro", {public = true})

-- Common library for testing
target("render.test.model.common")
	set_kind("static")
	set_default(false)

	add_files("test/model/common/**.cpp")
	add_includedirs("test/model/common/include", {public = true})
	add_headerfiles("test/model/common/include/(**.hpp)")
	add_packages("doctest", {public = true})

	add_deps("render", "vulkan.test-driver", {public = true})

-- Unit tests for model
target("render.test.model")
	set_kind("binary")
	set_default(false)

	add_packages("doctest")
	add_deps("render", "vulkan.test-driver", "render.test.model.common")

	set_runenv("TEMP_DIR", path.join(os.tmpdir()))

	for _, testfile in ipairs(os.files("test/model/*.cpp")) do
		add_tests(path.basename(testfile), {files = testfile})
	end
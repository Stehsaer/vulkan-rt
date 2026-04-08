-- Common utilities library, including formatting and error classes.

target("lib.common")
	set_kind("static")
	add_files("src/**.cpp")	

	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")
	add_packages("vulkan-hpp", {public = true})

	on_config(function (target)
		local size_t_byte = target:check_sizeof("std::size_t", {includes = "cstddef"})
		if not size_t_byte then
			raise("Failed to determine size of std::size_t")
		end

		if tonumber(size_t_byte) ~= 8 then
			raise("Only 64-bit platforms are supported")
		end
	end)

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
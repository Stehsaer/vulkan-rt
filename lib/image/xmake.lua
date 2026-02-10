-- Third-party libraries
includes("package/*.lua")
add_requires("stb_dxt", "bc7enc")

-- Image decoding library
target("lib.image")
	set_kind("static")

	add_files("src/*.cpp", "src/stb.c")
	add_includedirs("include", {public = true})
	add_deps("lib.common")
	
	add_packages("stb", "glm", {public = true})
	add_packages("stb_dxt", "bc7enc")

	add_rules("utils.bin2obj", {extensions = {".png"}})
	add_files("asset/blue-noise.png")

	for _, testfile in ipairs(os.files("test/*.cpp")) do
         add_tests(path.basename(testfile), {
             kind = "binary",
             files = {testfile, "test/asset/*.png"},
             packages = "doctest",
             defines = "DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN"
		})
     end
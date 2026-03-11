add_requires("tinyobjloader v2.0.0rc13")

target("main")
	set_kind("binary")

	add_rules("compile.slang")
	add_deps(
		"lib.common",
		"lib.image",
		"lib.scene",
		"vulkan.util",
		"vulkan.alloc",
		"vulkan.context"
	)
	
	add_files("src/**.cpp")
	add_includedirs("include")
	add_headerfiles("include/**.hpp")

	add_files("shader/*.slang")
	add_slang_deps("lighting", "algorithm")

	add_packages("tinyobjloader", "argparse")

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

	add_packages("rapidobj", "argparse")

	add_slang_deps("lighting", "algorithm")
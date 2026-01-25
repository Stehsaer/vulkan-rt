target("main")
	set_kind("binary")

	add_rules("compile.slang")
	add_deps(
		"lib.common",
		"lib.image",
		"vulkan.util",
		"vulkan.alloc",
		"vulkan.context"
	)
	
	add_files("src/**.cpp")
	add_includedirs("include")
	add_headerfiles("include/**.hpp")

	add_files("shader/first-shader.slang")
	add_files("asset/sample.jpg", {rule="utils.bin2obj"})
add_requires("tinyobjloader v2.0.0rc13")

target("main")
	set_kind("binary")

	add_deps(
		"lib.common",
		"lib.image",
		"lib.scene",
		"vulkan.util",
		"vulkan.alloc",
		"vulkan.context",
		"model.obj",
		"model.gltf",
		"render"
	)
	
	add_files("src/**.cpp")
	add_files("asset/**", {rule = "utils.bin2obj"})
	add_includedirs("include")
	add_headerfiles("include/**.hpp")

	add_packages("tinyobjloader", "argparse")

add_requires("tinyobjloader v2.0.0rc13")

target("main")
	set_kind("binary")

	add_deps(
		"common",
		"image",
		"scene",
		"vulkan.alloc",
		"vulkan.platform",
		"model.wavefront",
		"model.gltf",
		"render"
	)
	
	add_files("src/**.cpp")
	add_files("asset/**", {rule = "utils.bin2obj"})
	add_includedirs("include")
	add_headerfiles("include/**.hpp")

	add_packages("tinyobjloader", "argparse")

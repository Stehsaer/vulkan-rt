target("main")
	set_kind("binary")

	add_rules("compile.slang")
	add_deps("vulkan-window", "common", "vulkan-util")
	
	add_files("src/**.cpp")
	add_includedirs("include")
	add_headerfiles("include/**.hpp")

	add_files("shader/first-shader.slang")
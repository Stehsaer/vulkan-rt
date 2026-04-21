add_requires("tinyobjloader v2.0.0rc13")

-- Wavefront obj loading library
target("model.obj")
	set_kind("static")

	add_files("src/**.cpp")
	add_includedirs("include", {public = true})
	add_includedirs("impl")
	add_headerfiles("include/(**.hpp)")

	add_deps("lib.model", {public = true})
	add_packages("tinyobjloader")
	add_packages("libcoro", {public = true})

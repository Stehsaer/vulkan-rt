add_requires("fastgltf[cxx_standard=20] 0.9.0")

target("model.gltf")
	set_kind("static")

	add_files("src/**.cpp")
	add_headerfiles("include/(**.hpp)")
	add_includedirs("include", {public = true})
	add_includedirs("impl")
	
	add_deps("lib.model", {public = true})
	add_packages("fastgltf", "mio")
	add_packages("libcoro", {public = true})
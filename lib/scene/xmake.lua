-- Scene utilities

target("lib.scene")
	set_kind("static")

	add_files("src/*.cpp")
	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")

	add_packages("glm", {public = true})
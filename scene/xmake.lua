-- Scene utilities

target("scene")
	set_kind("static")

	add_files("src/*.cpp")
	add_includedirs("include", {public = true})
	add_headerfiles("include/(**.hpp)")

	add_deps("common", {public = true})
	add_packages("glm", {public = true})
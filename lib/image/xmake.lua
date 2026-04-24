-- Third-party libraries
includes("package/*.lua")
add_requires("stb_dxt", "bc7enc")
add_requires(
	"stb 2025.03.14",
	"libwebp v1.3.0"
)

-- Unit tests
includes("test")

-- Image decoding library
target("lib.image")
	set_kind("static")

	add_files("src/*.cpp", "src/stb.c")
	add_includedirs("include", {public = true})
	add_deps("lib.common", {public = true})
	
	add_packages("stb", "glm", {public = true})
	add_packages("stb_dxt", "bc7enc", "libwebp")

	add_files("asset/blue-noise.png", {rules = "utils.bin2obj"})
target("vulkan.platform.headless")
	set_kind("static")

	add_files("src/**.cpp")
	add_headerfiles("include/(**.hpp)")
	add_includedirs("include", {public = true})

	add_deps("vulkan.platform.common")
	add_deps("vulkan.common", {public = true})
	
	add_packages(
		"vulkan-hpp",
		{public = true}
	)

	if is_plat("linux") then
		add_files("platform/linux/**.cpp")
	end

	if is_plat("windows") then
		add_files("platform/windows/**.cpp")
	end

target("vulkan.platform.headless.test")
	set_kind("binary")
	set_default(false)

	add_files("test/**.cpp")
	add_deps("vulkan.platform.headless")
	add_packages("doctest")
	set_group("test")
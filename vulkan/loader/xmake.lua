-- Phony target for adding all vulkan packages
target("vulkan.loader")
	set_kind("phony")
	add_packages(
		"vulkan-loader",
		"vulkan-hpp",
		{public = true}
	)
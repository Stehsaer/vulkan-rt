local apis = {
	paths = {
		"slang.add_moduledirs"
	},
	values = {
		"slang.add_deps",
		"target.add_slang_deps"
	}
}

interp_add_scopeapis(apis)

rule("compile.slang")
	set_extensions(".slang")
	
	on_load(function (target)
		import("impl").load_rule(target)
	end)

	on_prepare_file(function (target, source_path, opt)
		import("impl").prepare_file(target, source_path, opt)
	end)

	on_build_file(function (target, source_path, opt)
		import("impl").build_file(target, source_path, opt)
	end)

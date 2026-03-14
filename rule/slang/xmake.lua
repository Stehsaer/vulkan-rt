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
	
	on_load("impl.load_rule")
	on_prepare_file("impl.prepare_file")
	on_build_file("impl.build_file")

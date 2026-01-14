import("lib.detect.find_program")
import("core.project.depend")
import("utils.progress")
import("core.project.config")
import("core.project.depend")
import("core.base.binutils")

local rule_name = "compile.slang"

function _get_path(target)
	local root_path = path.join(target:autogendir(), rule_name, config.get("mode"))
	local gen_header_root_path = path.join(root_path, "include")
	local gen_header_path = path.join(gen_header_root_path, "shader")
	local gen_temp_path = path.join(root_path, "src")

	return {
		header_root = gen_header_root_path,
		header = gen_header_path,
		temp = gen_temp_path,
	}
end

function _create_dir(paths)
	if not os.exists(paths.header) then
		os.mkdir(paths.header)
	end
	if not os.exists(paths.temp) then
		os.mkdir(paths.temp)
	end
end

function _get_tools()
	local tools = {
		slangc = find_program("slangc", {check="--help"}),
	}

	assert(tools.slangc, "Slang compiler not found, see README")

	return tools
end

function _get_filepaths(target, paths, source_path)
	local fileconfig = target:fileconfig(source_path)
	local module_name = fileconfig and fileconfig.module_name or string.gsub(path.basename(source_path), "[%.%-]", "_")

	local spv_temp_path = path.join(paths.temp, module_name .. ".spv") 			
	local header_output_path = path.join(paths.header, module_name .. ".hpp")
	local object_output_path = target:objectfile(module_name)

	return {
		source = source_path,
		spv = spv_temp_path,
		header = header_output_path,
		object = object_output_path,
		varname = module_name
	}
end

function _parse_dependencies(line)
	local deps = {}

	local colon_pos = string.find(line, ":")
	if not colon_pos then
		return deps
	end

	local deps_str = string.sub(line, colon_pos + 1)

	deps_str = deps_str
		:gsub(".*?:", "") 
		:match("^%s*(.-)%s*$") or ""

	for dep in deps_str:gmatch('"(.+)"') do
		dep = dep:gsub('^"(.*)"$', '%1')
		if dep ~= "" then
			table.insert(deps, dep)
		end
	end

	for dep in deps_str:gmatch("([^%s]+)") do
		dep = dep:gsub('^"(.*)"$', '%1')
		if dep ~= "" and dep:find("\"") == nil then
			table.insert(deps, dep)
		end
	end

	return deps
end

function _compile_spv(tools, files, debug)
	local optimization_flags = debug and {"-O0", "-g"} or {"-O3"}

	os.vrunv(tools.slangc, {
		files.source,
		"-target", "spirv",
		"-profile", "spirv_1_4",
		"-emit-spirv-directly",
		"-o", files.spv,
		"-depfile", files.spv .. ".d",
	})
end

function load_rule(target)
	local paths = _get_path(target)
	local public_include = target:extraconf("rules", rule_name, "public") or false
	target:add("includedirs", paths.header_root, {public=public_include})
end

function prepare_file(target, source_path, opt)

	local paths = _get_path(target)
	_create_dir(paths)

	local tools = _get_tools()
	local files = _get_filepaths(target, paths, source_path)

	local dependencies = os.exists(files.spv .. ".d") and _parse_dependencies(io.readfile(files.spv .. ".d")) or {}
	
	local target_name = string.gsub(target:name(), "[%.%-]", "_")
	local symbol_name = format("_asset_shader_%s_%s", target_name, files.varname)

	local header_file_template = [[
		#pragma once
		#include <span>

		extern "C" 
		{
			extern const std::byte %s_start;
			extern const std::byte %s_end;
		}

		namespace shader
		{
			inline static const std::span<const std::byte> %s = {
				&%s_start,
				&%s_end
			};
		}
	]];

	local header_file_content = format(
		header_file_template, 
		symbol_name, 
		symbol_name, 
		files.varname, 
		symbol_name, 
		symbol_name
	);

	depend.on_changed(function ()
		io.writefile(files.header, header_file_content)
	end,{
		files = table.join(dependencies, {files.source}),
		dependfile = target:dependfile(files.source),
		lastmtime = os.mtime(files.header),
		changed = target:is_rebuilt() or not os.exists(files.header)
	})
end

function build_file(target, source_path, opt)
	
	local paths = _get_path(target)
	local tools = _get_tools()
	local files = _get_filepaths(target, paths, source_path)
	local arch = target:arch()
	local plat = target:plat()

	local debug = target:extraconf("rules", rule_name, "debug") or false

	depend.on_changed(function() 
		progress.show(opt.progress, "${color.build.object}compiling.slang %s", source_path)
		_compile_spv(tools, files, debug)
		binutils.bin2obj(files.spv, files.object, {
			symbol_prefix = format("_asset_shader_%s_", string.gsub(target:name(), "[%.%-]", "_")),
			basename = files.varname,
			arch = arch,
			plat = plat
		})
	end, {
		files = files.header,
		dependfile = target:dependfile(files.header),
		lastmtime = os.mtime(files.object),
		changed = target:is_rebuilt() or not os.exists(files.object)
	})

	table.insert(target:objectfiles(), files.object)
end

# `compile.slang` Rule

## Description

This rule provides support for compiling `.slang` shader files using the Slang compiler. It defines a custom build step that converts Slang source files into SPIR-V binaries, which can then be used in Vulkan applications.

Example:
```lua
target("my_app")
	...
	add_rules("compile.slang")
	add_files("shaders/*.slang")
	...
```

When compiled, every `.slang` file will be compiled into a SPIR-V file, embedded into the target binary as `std::span<std::byte> shader::<module_name>`, where `module_name` is the shader file's basename. Thus you shouldn't have two shader files with the same name.

> Note: `.` and `-` inside the basename will be replaced with `_` to form a valid C++ identifier.

The definition of the binary can be accessed by including `shader/<module_name>.hpp` in C++ code. Example C++ snippet:
```cpp
#include "shader/my_shader.hpp"
void use_shader() {
	const auto shader_data = shader::my_shader;
	// Use shader_data as needed
}
```

## Dependency System

This rule implements a suite of custom description scope APIs, providing dependency management for slang modules.

Example:
```lua
slang("foo")
	add_moduledirs("foo")

slang("bar")
	add_moduledirs("bar")
	add_deps("foo")

target("main")
	...
	add_rules("compile.slang")
	add_slang_deps("bar")
	add_files("shader/*.slang")
	...
```

In the given example, all slang files in `shader/` will be compiled with both `foo` and `bar` modules available, as `bar` depends on `foo`.
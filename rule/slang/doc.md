# Slang compiling rule

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

When compiled, every `.slang` file will be compiled into a SPIR-V file, embedded into the target binary as `std::span<const std::byte> shader::<module_name>`, where `module_name` is the shader file's basename. Thus there shouldn't be two shader files with the same name (obviously!).

> Note: `.` and `-` inside the basename will be replaced with `_` to form a valid C++ identifier.

The definition of the binary can be accessed by including `shader/<module_name>.hpp` in C++ code. Example C++ snippet:

```cpp
#include "shader/my_shader.hpp"

void use_shader()
{
	const auto shader_data = shader::my_shader;
	// Use shader_data as needed
}
```

## Base directory

Additionally, if `base_dir` is specified when using `add_files`, the header path and the namespace will include the subdirectory of the shader.

Example:

```lua
add_files("shaders/**.slang", {base_dir = path.absolute("shaders")})
```

With the above example, a shader file `shaders/my-function/test.slang` will generate a header file which can be included using `shaders/my-function/test.hpp`, and the C++ name will be `shader::my_function::test`.

```cpp
#include "shader/my-function/test.hpp"

void use_shader()
{
	const auto shader_data = shader::my_function::test;
	// Use shader_data as needed
}
```

> Note: Always use `path.absolute` when specifying `base_dir`.

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

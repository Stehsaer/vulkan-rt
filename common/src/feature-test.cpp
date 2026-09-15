#include <version>

static_assert(__cpp_lib_containers_ranges >= 202202L, "Missing support for std::from_range");
static_assert(__cpp_lib_format_ranges >= 202207L, "Missing support for std::format with ranges");
static_assert(__cpp_lib_ranges >= 201911L, "Missing support for the C++20 ranges library");
static_assert(__cpp_lib_ranges_to_container >= 202202L, "Missing support for std::ranges::to");
static_assert(__cpp_lib_format >= 202110L, "Missing support for std::format");
static_assert(__cpp_lib_expected >= 202211L, "Missing support for std::expected");
static_assert(__cpp_lib_span >= 202002L, "Missing support for std::span");
static_assert(__cpp_lib_to_underlying >= 202102L, "Missing support for std::to_underlying");
static_assert(__cpp_consteval >= 201811L, "Missing support for consteval");
static_assert(__cpp_concepts >= 201907L, "Missing support for concepts");
static_assert(__cpp_lib_concepts >= 202002L, "Missing support for library concepts");
static_assert(__cpp_size_t_suffix >= 202011L, "Missing _zu suffix for size_t");

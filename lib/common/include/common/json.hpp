#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <nlohmann/adl_serializer.hpp>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

using Json = nlohmann::basic_json<
	std::map,
	std::vector,
	std::string,
	bool,
	int64_t,
	uint64_t,
	double,
	std::allocator,
	nlohmann::adl_serializer,
	std::vector<std::byte>,
	void
>;

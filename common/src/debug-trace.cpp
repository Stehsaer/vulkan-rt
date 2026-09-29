#include "common/container/debug-trace.hpp"

#include <initializer_list>
#include <libassert/assert.hpp>
#include <memory>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace debug
{
	Trace::Trace(std::initializer_list<Key> value) noexcept
	{
		for (const auto& pair : value)
			if (pair.has_value()) data.emplace(pair.pair());
	}

	[[nodiscard]]
	std::pair<std::string, std::shared_ptr<Trace::Variant>> Key::pair() const noexcept
	{
		ASSERT(data.has_value());
		return {this->key, data.value()};
	}

	template <typename T>
	static nlohmann::json variant_to_json(const T& value) noexcept;

	template <>
	nlohmann::json variant_to_json(const std::unique_ptr<Value>& value) noexcept;

	template <>
	nlohmann::json variant_to_json(const std::unique_ptr<Trace>& value) noexcept;

	template <>
	nlohmann::json variant_to_json(const std::vector<std::unique_ptr<Value>>& value) noexcept;

	template <>
	nlohmann::json variant_to_json(const std::vector<std::unique_ptr<Trace>>& value) noexcept;

	template <>
	nlohmann::json Trace::format() const noexcept
	{
		nlohmann::json json;
		for (const auto& [name, variant] : data)
			json[name] = std::visit([](const auto& value) { return variant_to_json(value); }, *variant);

		return json;
	}

	template <>
	nlohmann::json variant_to_json(const std::unique_ptr<Value>& value) noexcept
	{
		nlohmann::json json;
		json[value->type()] = value->stringify();
		return json;
	}

	template <>
	nlohmann::json variant_to_json(const std::unique_ptr<Trace>& value) noexcept
	{
		return value->format<nlohmann::json>();
	}

	template <>
	nlohmann::json variant_to_json(const std::vector<std::unique_ptr<Value>>& value) noexcept
	{
		nlohmann::json list = nlohmann::json::array();
		for (const auto& value : value) list.emplace_back(variant_to_json(value));
		return list;
	}

	template <>
	nlohmann::json variant_to_json(const std::vector<std::unique_ptr<Trace>>& value) noexcept
	{
		nlohmann::json list = nlohmann::json::array();
		for (const auto& value : value) list.emplace_back(variant_to_json(value));
		return list;
	}
}

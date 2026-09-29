#pragma once

#include <concepts>
#include <cstddef>
#include <flat_map>
#include <initializer_list>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace debug
{
	class Value;
	class Key;

	///
	/// @brief Key-value tree, tracking debug information
	/// @details
	/// #### Create
	/// Follow this intuitive way to create a trace:
	/// ```cpp
	/// using debug::operator""_key;
	///
	/// debug::Trace trace = {
	/// 	"foo"_key = debug::value::Number(0),
	/// 	"bar"_key = debug::value::Boolean(true),
	/// 	"test"_key = debug::value::String("Hello World")
	/// };
	/// ```
	///
	/// #### Formatting
	/// Use `Trace::format()` to format the trace into desired target. Currently supports:
	/// - Nlohmann's JSON (`nlohmann::json`)
	///
	class Trace
	{
	  public:

		using Variant = std::variant<
			std::unique_ptr<Trace>,
			std::vector<std::unique_ptr<Value>>,
			std::unique_ptr<Value>,
			std::vector<std::unique_ptr<Trace>>
		>;

		Trace(std::initializer_list<Key> value) noexcept;

		template <std::same_as<Key>... T>
		Trace(T... keys) noexcept :
			Trace({std::move(keys)...})
		{}

		///
		/// @brief Format as given structure. Currently supports `nlohmann::json`.
		///
		/// @tparam T Target type
		/// @return Formatted structure
		///
		template <typename T>
		[[nodiscard]]
		T format() const noexcept;

	  private:

		std::flat_map<std::string, std::shared_ptr<Variant>> data;

	  public:

		Trace(const Trace&) = default;
		Trace(Trace&&) = default;
		Trace& operator=(const Trace&) = default;
		Trace& operator=(Trace&&) = default;
	};

	///
	/// @brief Debug value interface
	///
	class Value
	{
	  public:

		///
		/// @brief Stringify the value
		///
		/// @return String representation of the value
		///
		[[nodiscard]]
		virtual std::string stringify() const noexcept = 0;

		///
		/// @brief Type of the value
		///
		/// @return String view of type
		///
		[[nodiscard]]
		virtual std::string_view type() const noexcept = 0;

		Value() = default;
		virtual ~Value() = default;

		Value(const Value&) = default;
		Value(Value&&) = default;
		Value& operator=(const Value&) = default;
		Value& operator=(Value&&) = default;
	};

	///
	/// @brief Tree-like value, can be resolved to `Trace`
	///
	class TreeValue
	{
	  public:

		///
		/// @brief Convert to `Trace`
		///
		/// @return Converted `Trace`, gurantees success
		///
		[[nodiscard]]
		virtual Trace convert() const noexcept = 0;

		TreeValue() = default;
		virtual ~TreeValue() = default;

		TreeValue(const TreeValue&) = default;
		TreeValue(TreeValue&&) = default;
		TreeValue& operator=(const TreeValue&) = default;
		TreeValue& operator=(TreeValue&&) = default;
	};

	///
	/// @brief Temporary object for creating key-value pairs
	/// @details Accept a single/optional/range of `Value`/`TreeValue`/`Trace` object
	///
	/// @note See documentation of `Trace` for usage
	///
	class Key
	{
	  public:

		explicit Key(std::string key) :
			key(std::move(key))
		{}

		template <std::derived_from<Value> T>
		[[nodiscard]]
		Key& operator=(T value) noexcept
		{
			data = std::make_shared<Trace::Variant>(std::make_unique<T>(std::move(value)));
			return *this;
		}

		template <std::derived_from<TreeValue> T>
		[[nodiscard]]
		Key& operator=(T value) noexcept
		{
			data = std::make_shared<Trace::Variant>(std::make_unique<Trace>(value.convert()));
			return *this;
		}

		template <std::derived_from<Value> T>
		[[nodiscard]]
		Key& operator=(std::optional<T> value) noexcept
		{
			if (!value) return *this;

			data = std::make_shared<Trace::Variant>(std::make_unique<T>(std::move(*value)));
			return *this;
		}

		template <std::derived_from<TreeValue> T>
		[[nodiscard]]
		Key& operator=(std::optional<T> value) noexcept
		{
			if (!value) return *this;

			data = std::make_shared<Trace::Variant>(std::make_unique<Trace>(value->convert()));
			return *this;
		}

		[[nodiscard]]
		Key& operator=(Trace info) noexcept
		{
			data = std::make_shared<Trace::Variant>(std::make_unique<Trace>(std::move(info)));
			return *this;
		}

		[[nodiscard]]
		Key& operator=(std::optional<Trace> info) noexcept
		{
			if (!info) return *this;

			data = std::make_shared<Trace::Variant>(std::make_unique<Trace>(std::move(*info)));
			return *this;
		}

		template <std::ranges::input_range Range>
			requires(std::derived_from<std::ranges::range_value_t<Range>, Value>)
		[[nodiscard]]
		Key& operator=(Range&& range) noexcept
		{
			data = std::make_shared<Trace::Variant>(std::vector(
				std::from_range,
				std::forward<Range>(range)
					| std::views::transform([](auto&& value) -> std::unique_ptr<Value> {
						  return std::make_unique<std::remove_cvref_t<decltype(value)>>(
							  std::forward<decltype(value)>(value)
						  );
					  })
			));

			return *this;
		}

		template <std::ranges::input_range Range>
			requires(std::derived_from<std::ranges::range_value_t<Range>, TreeValue>)
		[[nodiscard]]
		Key& operator=(Range&& range) noexcept
		{
			data = std::make_shared<Trace::Variant>(std::vector(
				std::from_range,
				std::forward<Range>(range) | std::views::transform([](auto&& value) {
					return std::make_unique<Trace>(value.convert());
				})
			));

			return *this;
		}

		template <std::ranges::input_range Range>
			requires(std::convertible_to<std::ranges::range_value_t<Range>, Trace>)
		[[nodiscard]]
		Key& operator=(Range&& range) noexcept
		{
			data = std::make_shared<Trace::Variant>(std::vector(
				std::from_range,
				std::forward<Range>(range) | std::views::transform([](auto&& value) {
					return std::make_unique<Trace>(value);
				})
			));

			return *this;
		}

		[[nodiscard]]
		bool has_value() const noexcept
		{
			return data.has_value();
		}

		[[nodiscard]]
		std::pair<std::string, std::shared_ptr<Trace::Variant>> pair() const noexcept;

	  private:

		std::string key;
		std::optional<std::shared_ptr<Trace::Variant>> data = std::nullopt;

	  public:

		Key(const Key&) = default;
		Key(Key&&) = default;
		Key& operator=(const Key&) = default;
		Key& operator=(Key&&) = default;
	};

	///
	/// @brief Literal for a key
	///
	inline Key operator""_key(const char* key, size_t length) noexcept
	{
		return Key(std::string(key, length));
	}
}

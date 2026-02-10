#pragma once

#include <cassert>
#include <cstdint>
#include <memory>
#include <ranges>
#include <span>

namespace util
{
	///
	/// @brief Converts an non-temporary object to a byte span, which views the object's memory
	///
	/// @tparam T Type of the object
	/// @param object Object to convert
	/// @return Byte span representing the object's memory
	///
	template <typename T>
	[[nodiscard]]
	std::span<const std::byte> object_as_bytes(const T& object) noexcept
	{
		auto ptr = reinterpret_cast<const std::byte*>(std::addressof(object));
		return {ptr, sizeof(T)};
	}

	///
	/// @brief Converts a contiguous range to a byte span, which views the range's memory
	///
	/// @tparam T Type of the range
	/// @param range Range to convert
	/// @return Byte span representing the range's memory
	///
	template <std::ranges::contiguous_range T>
	[[nodiscard]]
	std::span<const std::byte> as_bytes(const T& range) noexcept
	{
		auto ptr = reinterpret_cast<const std::byte*>(std::ranges::data(range));
		auto size = sizeof(std::ranges::range_value_t<T>) * std::ranges::size(range);
		return {ptr, size};
	}

	///
	/// @brief Converts a contiguous range to a writable byte span, which views the range's memory
	///
	/// @tparam T Type of the range
	/// @param range Range to convert
	/// @return Writable byte span representing the range's memory
	///
	template <std::ranges::contiguous_range T>
	[[nodiscard]]
	std::span<std::byte> as_writable_bytes(T& range) noexcept
	{
		auto ptr = reinterpret_cast<std::byte*>(std::ranges::data(range));
		auto size = sizeof(std::ranges::range_value_t<T>) * std::ranges::size(range);
		return {ptr, size};
	}

	///
	/// @brief Converts a byte span back to a span of the specified type
	/// @note Uses assertions internally to ensure size and alignment are correct. Check carefully before use.
	///
	/// @tparam T Target type, should be const
	/// @param bytes Byte span to convert
	/// @return Span of the target type representing the byte span's memory
	///
	template <typename T>
		requires(std::same_as<T, const std::remove_extent_t<T>>)
	[[nodiscard]]
	std::span<const T> from_bytes(std::span<const std::byte> bytes) noexcept
	{
		assert(bytes.size() % sizeof(T) == 0);
		assert(reinterpret_cast<uintptr_t>(bytes.data()) % alignof(T) == 0);

		auto ptr = reinterpret_cast<const T*>(std::assume_aligned<alignof(T)>(bytes.data()));
		auto size = bytes.size() / sizeof(T);
		return {ptr, size};
	}

	///
	/// @brief Converts a byte span back to a span of the specified type
	/// @note Uses assertions internally to ensure size and alignment are correct. Check carefully before use.
	///
	/// @tparam T Target type, should be non-const
	/// @param bytes Byte span to convert
	/// @return Span of the target type representing the byte span's memory
	///
	template <typename T>
		requires(std::same_as<T, std::remove_extent_t<T>>)
	[[nodiscard]]
	std::span<T> from_writable_bytes(std::span<std::byte> bytes) noexcept
	{
		assert(bytes.size() % sizeof(T) == 0);
		assert(reinterpret_cast<uintptr_t>(bytes.data()) % alignof(T) == 0);

		auto ptr = reinterpret_cast<T*>(std::assume_aligned<alignof(T)>(bytes.data()));
		auto size = bytes.size() / sizeof(T);
		return {ptr, size};
	}
}
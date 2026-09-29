#pragma once

#include "common/container/error.hpp"
#include "debug-trace.hpp"

#include <concepts>
#include <cstddef>
#include <format>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace debug::value
{
	///
	/// @brief Numbers. Accepts any integer or floating-point number
	///
	class Number : public Value
	{
	  public:

		template <std::floating_point T>
		explicit Number(T value) noexcept :
			value_str(std::format("{:.15g}", value))
		{
			switch (sizeof(T))
			{
			case 2:
				type_str = "f16";
				break;
			case 4:
				type_str = "f32";
				break;
			case 8:
				type_str = "f64";
				break;
			default:
				type_str = "f?";
				break;
			};
		}

		template <std::unsigned_integral T>
		explicit Number(T value) noexcept :
			value_str(std::format("{:d}", value))
		{
			switch (sizeof(T))
			{
			case 1:
				type_str = "u8";
				break;
			case 2:
				type_str = "u16";
				break;
			case 4:
				type_str = "u32";
				break;
			case 8:
				type_str = "u64";
				break;
			default:
				type_str = "u?";
				break;
			};
		}

		template <std::signed_integral T>
		explicit Number(T value) noexcept :
			value_str(std::format("{:d}", value))
		{
			switch (sizeof(T))
			{
			case 1:
				type_str = "i8";
				break;
			case 2:
				type_str = "i16";
				break;
			case 4:
				type_str = "i32";
				break;
			case 8:
				type_str = "i64";
				break;
			default:
				type_str = "i?";
				break;
			};
		}

		[[nodiscard]]
		std::string stringify() const noexcept override
		{
			return value_str;
		}

		[[nodiscard]]
		std::string_view type() const noexcept override
		{
			return type_str;
		}

	  private:

		std::string_view type_str;
		std::string value_str;

	  public:

		Number(const Number&) = default;
		Number(Number&&) = default;
		Number& operator=(const Number&) = default;
		Number& operator=(Number&&) = default;
	};

	///
	/// @brief String value. Accepts `std::string` or those convertible to `std::string`
	///
	class String : public Value
	{
	  public:

		explicit String(std::string str) :
			value(std::move(str))
		{}

		[[nodiscard]]
		std::string stringify() const noexcept override;

		[[nodiscard]]
		std::string_view type() const noexcept override
		{
			return "string";
		}

	  private:

		std::string value;

	  public:

		String(const String&) = default;
		String(String&&) = default;
		String& operator=(const String&) = default;
		String& operator=(String&&) = default;
	};

	///
	/// @brief Boolean value. Accepts boolean or integer values. Non-zero integer values are treated as
	/// `true`.
	///
	class Boolean : public Value
	{
	  public:

		explicit Boolean(bool value) :
			value(value)
		{}

		///
		/// @brief Construct a boolean from an integer. Non-zero integer values are treated as `true`
		///
		/// @tparam T Type of the integer
		/// @param value Value of the integer
		///
		template <std::integral T>
		explicit Boolean(T value) :
			value(value != T(0))
		{}

		[[nodiscard]]
		std::string stringify() const noexcept override
		{
			return value ? "true" : "false";
		}

		[[nodiscard]]
		std::string_view type() const noexcept override
		{
			return "bool";
		}

	  private:

		bool value;

	  public:

		Boolean(const Boolean&) = default;
		Boolean(Boolean&&) = default;
		Boolean& operator=(const Boolean&) = default;
		Boolean& operator=(Boolean&&) = default;
	};

	///
	/// @brief Byte stream. Accepts `std::span<const std::byte>`.
	/// @note This object saves a copy of binary data. Beware of memory usage
	///
	class ByteStream : public Value
	{
	  public:

		explicit ByteStream(std::span<const std::byte> binary_data) :
			binary_data(std::from_range, binary_data)
		{}

		[[nodiscard]]
		std::string stringify() const noexcept override;

		[[nodiscard]]
		std::string_view type() const noexcept override
		{
			return "octet-stream";
		}

	  private:

		std::vector<std::byte> binary_data;

	  public:

		ByteStream(const ByteStream&) = delete;
		ByteStream(ByteStream&&) = default;
		ByteStream& operator=(const ByteStream&) = delete;
		ByteStream& operator=(ByteStream&&) = default;
	};

	///
	/// @brief Error. Accepts `Error`.
	///
	class Error : public TreeValue
	{
	  public:

		Error(::Error err) :
			err(std::move(err))
		{}

		[[nodiscard]]
		Trace convert() const noexcept override;

	  private:

		::Error err;

	  public:

		Error(const Error&) = default;
		Error(Error&&) = default;
		Error& operator=(const Error&) = default;
		Error& operator=(Error&&) = default;
	};
}

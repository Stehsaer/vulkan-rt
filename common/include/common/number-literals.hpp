#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace impl
{
	template <typename T>
	consteval T literal(unsigned long long val)
	{
		if (val > std::numeric_limits<T>::max())
			throw std::logic_error("");
		else
			return val;
	}
}

consteval uint8_t operator""_u8(unsigned long long val)
{
	return impl::literal<uint8_t>(val);
}

consteval uint16_t operator""_u16(unsigned long long val)
{
	return impl::literal<uint16_t>(val);
}

consteval uint32_t operator""_u32(unsigned long long val)
{
	return impl::literal<uint32_t>(val);
}

consteval uint64_t operator""_u64(unsigned long long val)
{
	return impl::literal<uint64_t>(val);
}

consteval int8_t operator""_i8(unsigned long long val)
{
	return impl::literal<int8_t>(val);
}

consteval int16_t operator""_i16(unsigned long long val)
{
	return impl::literal<int16_t>(val);
}

consteval int32_t operator""_i32(unsigned long long val)
{
	return impl::literal<int32_t>(val);
}

consteval int64_t operator""_i64(unsigned long long val)
{
	return impl::literal<int64_t>(val);
}

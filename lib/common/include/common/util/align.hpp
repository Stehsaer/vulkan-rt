#pragma once

#include <concepts>
#include <libassert/assert.hpp>
#include <type_traits>

namespace util
{
	///
	/// @brief Align unsigned integral address. The address will be aligned to the nearest larger address
	/// that meets the alignment.
	///
	/// @tparam Taddr Type of the address value
	/// @tparam Talignment Type of the alignment value
	///
	/// @param addr Address, must be an unsigned integer
	/// @param alignment Alignment, must be an unsigned integer
	/// @return Aligned address value
	///
	template <std::unsigned_integral Taddr, std::unsigned_integral Talignment>
	constexpr std::common_type_t<Taddr, Talignment> align_address(Taddr addr, Talignment alignment) noexcept
	{
		ASSUME(std::has_single_bit(alignment));

		using CommonType = std::common_type_t<Taddr, Talignment>;

		return (static_cast<CommonType>(addr) + static_cast<CommonType>(alignment) - CommonType(1))
			& ~(static_cast<CommonType>(alignment) - CommonType(1));
	}
}

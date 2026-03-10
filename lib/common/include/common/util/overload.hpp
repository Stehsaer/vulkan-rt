#pragma once

namespace util
{
	///
	/// @brief Overload template, primarily used for `std::visit`
	///
	template <typename... Ts>
	struct Overload : Ts...
	{
		using Ts::operator()...;
	};

	template <class... Ts>
	Overload(Ts...) -> Overload<Ts...>;
}

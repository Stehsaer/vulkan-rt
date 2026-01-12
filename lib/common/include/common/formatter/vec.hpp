///
/// @file vec.hpp
/// @brief Provides std::formatter specializations for vector types (e.g., glm::vec2, glm::vec3, glm::vec4)
/// @note Detects vector types based on the presence of x, y, z, w members
///

#pragma once

#include <format>

namespace vec_formatter
{
	template <typename T>
	concept Vec4Type =
		requires(T v) {
			v.x;
			v.y;
			v.z;
			v.w;
		}
		&& std::same_as<decltype(T().x), decltype(T().y)>
		&& std::same_as<decltype(T().x), decltype(T().z)>
		&& std::same_as<decltype(T().x), decltype(T().w)>;

	template <typename T>
	concept Vec3Type =
		requires(T v) {
			v.x;
			v.y;
			v.z;
		}
		&& !Vec4Type<T>
		&& std::same_as<decltype(T().x), decltype(T().y)>
		&& std::same_as<decltype(T().x), decltype(T().z)>;

	template <typename T>
	concept Vec2Type = requires(T v) {
		v.x;
		v.y;
	} && !Vec3Type<T> && !Vec4Type<T> && std::same_as<decltype(T().x), decltype(T().y)>;

	template <typename T>
	using BaseFormatterForVec = std::formatter<decltype(T().x), char>;
}

template <vec_formatter::Vec2Type T>
struct std::formatter<T, char> : public vec_formatter::BaseFormatterForVec<T>
{
	auto format(const T& value, std::format_context& ctx) const
	{
		std::format_to(ctx.out(), "(");
		vec_formatter::BaseFormatterForVec<T>::format(value.x, ctx);
		std::format_to(ctx.out(), ", ");
		vec_formatter::BaseFormatterForVec<T>::format(value.y, ctx);
		std::format_to(ctx.out(), ")");
		return ctx.out();
	}
};

template <vec_formatter::Vec3Type T>
struct std::formatter<T, char> : public vec_formatter::BaseFormatterForVec<T>
{
	auto format(const T& value, std::format_context& ctx) const
	{
		std::format_to(ctx.out(), "(");
		vec_formatter::BaseFormatterForVec<T>::format(value.x, ctx);
		std::format_to(ctx.out(), ", ");
		vec_formatter::BaseFormatterForVec<T>::format(value.y, ctx);
		std::format_to(ctx.out(), ", ");
		vec_formatter::BaseFormatterForVec<T>::format(value.z, ctx);
		std::format_to(ctx.out(), ")");
		return ctx.out();
	}
};

template <vec_formatter::Vec4Type T>
struct std::formatter<T, char> : public vec_formatter::BaseFormatterForVec<T>
{
	auto format(const T& value, std::format_context& ctx) const
	{
		std::format_to(ctx.out(), "(");
		vec_formatter::BaseFormatterForVec<T>::format(value.x, ctx);
		std::format_to(ctx.out(), ", ");
		vec_formatter::BaseFormatterForVec<T>::format(value.y, ctx);
		std::format_to(ctx.out(), ", ");
		vec_formatter::BaseFormatterForVec<T>::format(value.z, ctx);
		std::format_to(ctx.out(), ", ");
		vec_formatter::BaseFormatterForVec<T>::format(value.w, ctx);
		std::format_to(ctx.out(), ")");
		return ctx.out();
	}
};
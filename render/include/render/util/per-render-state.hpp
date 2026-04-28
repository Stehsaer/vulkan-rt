#pragma once

#include "model/material.hpp"

#include <concepts>
#include <ranges>

namespace render
{
	///
	/// @brief Template struct to hold per-render-state data for different alpha modes and double-sidedness.
	///
	/// @tparam T Type of elements
	///
	template <typename T>
	struct PerRenderState
	{
		T opaque_single_sided;
		T opaque_double_sided;
		T masked_single_sided;
		T masked_double_sided;

		///
		/// @brief Get reference of the element corresponding to the given alpha mode and double-sidedness.
		///
		/// @param alpha_mode Alpha mode
		/// @param double_sided Double-sidedness. `true` for double-sided
		/// @return Reference to the element
		///
		[[nodiscard]]
		auto& operator[](this auto&& self, model::AlphaMode alpha_mode, bool double_sided) noexcept
		{
			switch (alpha_mode)
			{
			case model::AlphaMode::Opaque:
				return double_sided ? self.opaque_double_sided : self.opaque_single_sided;

			case model::AlphaMode::Blend:
			case model::AlphaMode::Mask:
			default:
				return double_sided ? self.masked_double_sided : self.masked_single_sided;
			}
		}

		///
		/// @brief Get reference of the element corresponding to the given material mode.
		///
		/// @param mode Material mode
		/// @return Reference to the element
		///
		[[nodiscard]]
		auto& operator[](this auto&& self, model::Material::Mode mode) noexcept
		{
			return self[mode.alpha_mode, mode.double_sided];
		}

		///
		/// @brief Get an array of references to all elements in the struct.
		///
		/// @return Array of references (reference-wrapper) to all elements
		///
		[[nodiscard]]
		auto as_ref_array(this auto&& self) noexcept
		{
			return std::to_array({
				std::ref(self.opaque_single_sided),
				std::ref(self.opaque_double_sided),
				std::ref(self.masked_single_sided),
				std::ref(self.masked_double_sided),
			});
		}

		///
		/// @brief Get an array of all elements in the struct.
		/// @warning The returned array holds copies of the elements
		///
		/// @return Array of all elements
		///
		[[nodiscard]]
		std::array<T, 4> as_array() const noexcept
			requires std::copy_constructible<T>
		{
			return std::to_array({
				opaque_single_sided,
				opaque_double_sided,
				masked_single_sided,
				masked_double_sided,
			});
		}

		///
		/// @brief Get a view of references to all elements in the struct.
		///
		/// @return View of references to all elements
		///
		[[nodiscard]]
		auto all(this auto&& self) noexcept
		{
			return self.as_ref_array() | std::views::transform([](auto&& ref) -> auto& { return ref.get(); });
		}

		///
		/// @brief Construct an instance by providing a constructor which takes alpha mode and
		/// double-sidedness as parameters.
		///
		/// @tparam F Constructor type. Must be invocable with `model::AlphaMode` and `bool` parameters, and
		/// return a type convertible to `T` or `std::expected<T, Error>`.
		/// @param ctor Constructor function or functor
		/// @return Constructed `PerRenderState` instance
		///
		template <std::invocable<model::AlphaMode, bool> F>
			requires std::convertible_to<std::invoke_result_t<F, model::AlphaMode, bool>, T>
		[[nodiscard]]
		static PerRenderState from_ctor(
			F ctor
		) noexcept(std::is_nothrow_invocable_v<F, model::AlphaMode, bool>)
		{
			return PerRenderState{
				.opaque_single_sided = ctor(model::AlphaMode::Opaque, false),
				.opaque_double_sided = ctor(model::AlphaMode::Opaque, true),
				.masked_single_sided = ctor(model::AlphaMode::Mask, false),
				.masked_double_sided = ctor(model::AlphaMode::Mask, true),
			};
		}

		///
		/// @brief Construct an instance by providing a constructor which takes alpha mode and
		/// double-sidedness as parameters, and returns a `std::expected<T, Error>`.
		///
		/// @tparam F Constructor type. Must be invocable with `model::AlphaMode` and `bool` parameters, and
		/// return a `std::expected<T, Error>`.
		/// @param ctor Constructor function or functor
		/// @retval Error When any of the constructor calls fail. The error message is forwarded with
		/// additional context about which mode failed.
		/// @retval PerRenderState Constructed `PerRenderState` instance when all constructor calls succeed.
		///
		template <std::invocable<model::AlphaMode, bool> F>
			requires std::is_same_v<std::invoke_result_t<F, model::AlphaMode, bool>, std::expected<T, Error>>
		[[nodiscard]]
		static std::expected<PerRenderState, Error> from_ctor(
			F ctor
		) noexcept(std::is_nothrow_invocable_v<F, model::AlphaMode, bool>)
		{
			auto opaque_single_sided_result = ctor(model::AlphaMode::Opaque, false);
			auto opaque_double_sided_result = ctor(model::AlphaMode::Opaque, true);
			auto masked_single_sided_result = ctor(model::AlphaMode::Mask, false);
			auto masked_double_sided_result = ctor(model::AlphaMode::Mask, true);

			if (!opaque_single_sided_result)
			{
				return opaque_single_sided_result.error()
					.forward("Create render state failed", "mode = opaque single-sided");
			}

			if (!opaque_double_sided_result)
			{
				return opaque_double_sided_result.error()
					.forward("Create render state failed", "mode = opaque double-sided");
			}

			if (!masked_single_sided_result)
			{
				return masked_single_sided_result.error()
					.forward("Create render state failed", "mode = masked single-sided");
			}

			if (!masked_double_sided_result)
			{
				return masked_double_sided_result.error()
					.forward("Create render state failed", "mode = masked double-sided");
			}

			return PerRenderState{
				.opaque_single_sided = std::move(*opaque_single_sided_result),
				.opaque_double_sided = std::move(*opaque_double_sided_result),
				.masked_single_sided = std::move(*masked_single_sided_result),
				.masked_double_sided = std::move(*masked_double_sided_result),
			};
		}

		///
		/// @brief Construct an instance by providing a constructor which takes no parameters.
		///
		/// @tparam F Constructor type. Must be invocable with no parameters, and return a type convertible to
		/// `T`
		/// @param ctor Constructor function or functor
		/// @return Constructed `PerRenderState` instance
		///
		template <std::invocable<void> F>
			requires std::convertible_to<std::invoke_result_t<F>, T>
		[[nodiscard]]
		static PerRenderState from_ctor(F ctor) noexcept(std::is_nothrow_invocable_v<F>)
		{
			return PerRenderState{
				.opaque_single_sided = ctor(),
				.opaque_double_sided = ctor(),
				.masked_single_sided = ctor(),
				.masked_double_sided = ctor(),
			};
		}

		///
		/// @brief Construct an instance by providing a constructor which takes no parameters, and returns a
		/// `std::expected<T, Error>`.
		///
		/// @tparam F Constructor type. Must be invocable with no parameters, and return a `std::expected<T,
		/// Error>`.
		/// @param ctor Constructor function or functor
		/// @retval Error When any of the constructor calls fail. The error message is forwarded with
		/// additional context about which mode failed.
		/// @retval PerRenderState Constructed `PerRenderState` instance when all constructor calls succeed.
		///
		template <std::invocable<void> F>
			requires std::is_same_v<std::invoke_result_t<F>, std::expected<T, Error>>
		[[nodiscard]]
		static std::expected<PerRenderState, Error> from_ctor(F ctor) noexcept(std::is_nothrow_invocable_v<F>)
		{
			auto opaque_single_sided_result = ctor();
			auto opaque_double_sided_result = ctor();
			auto masked_single_sided_result = ctor();
			auto masked_double_sided_result = ctor();

			if (!opaque_single_sided_result)
			{
				return opaque_single_sided_result.error()
					.forward("Create render state failed", "mode = opaque single-sided");
			}

			if (!opaque_double_sided_result)
			{
				return opaque_double_sided_result.error()
					.forward("Create render state failed", "mode = opaque double-sided");
			}

			if (!masked_single_sided_result)
			{
				return masked_single_sided_result.error()
					.forward("Create render state failed", "mode = masked single-sided");
			}

			if (!masked_double_sided_result)
			{
				return masked_double_sided_result.error()
					.forward("Create render state failed", "mode = masked double-sided");
			}

			return PerRenderState{
				.opaque_single_sided = std::move(*opaque_single_sided_result),
				.opaque_double_sided = std::move(*opaque_double_sided_result),
				.masked_single_sided = std::move(*masked_single_sided_result),
				.masked_double_sided = std::move(*masked_double_sided_result),
			};
		}

		///
		/// @brief Construct an instance by providing constructor arguments for `T`. The same arguments are
		/// used for all elements.
		///
		/// @param args Constructor arguments for `T`. The same arguments are used for all elements.
		/// @return Constructed `PerRenderState` instance
		///
		template <typename... Args>
			requires std::constructible_from<T, Args...>
		[[nodiscard]]
		static PerRenderState from_args(Args&&... args) noexcept(std::is_nothrow_constructible_v<T, Args...>)
		{
			return PerRenderState{
				.opaque_single_sided = T(std::forward<Args>(args)...),
				.opaque_double_sided = T(std::forward<Args>(args)...),
				.masked_single_sided = T(std::forward<Args>(args)...),
				.masked_double_sided = T(std::forward<Args>(args)...),
			};
		}

		///
		/// @brief Map a function over all elements in the struct, returning a new `PerRenderState` with the
		/// mapped values.
		///
		/// @tparam F Mapping function type. Must be invocable with a `const T&` parameter, and return a type
		/// convertible to the value type of the resulting `PerRenderState`.
		/// @param func Mapping function or functor
		/// @return New `PerRenderState` instance with mapped values
		///
		template <std::invocable<const T&> F>
		[[nodiscard]]
		auto map(F func) const noexcept(std::is_nothrow_invocable_v<F, const T&>)
		{
			return PerRenderState<std::invoke_result_t<F, const T&>>{
				.opaque_single_sided = std::invoke(func, opaque_single_sided),
				.opaque_double_sided = std::invoke(func, opaque_double_sided),
				.masked_single_sided = std::invoke(func, masked_single_sided),
				.masked_double_sided = std::invoke(func, masked_double_sided),
			};
		}

		///
		/// @brief Convert the `PerRenderState` to a different value type.
		///
		/// @tparam Ty The target value type.
		/// @return A new `PerRenderState` instance with the converted values.
		///
		template <typename Ty>
			requires std::convertible_to<T, Ty>
		[[nodiscard]]
		PerRenderState<Ty> to() const noexcept(std::is_nothrow_convertible_v<T, Ty>)
		{
			return PerRenderState<Ty>{
				.opaque_single_sided = static_cast<Ty>(opaque_single_sided),
				.opaque_double_sided = static_cast<Ty>(opaque_double_sided),
				.masked_single_sided = static_cast<Ty>(masked_single_sided),
				.masked_double_sided = static_cast<Ty>(masked_double_sided),
			};
		}
	};
}

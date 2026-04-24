#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <variant>

namespace util
{
	///
	/// @brief Tagged value type that associates a value with a compile-time tag.
	///
	/// @tparam TagValue A compile-time tag that identifies the type of the value.
	/// @tparam V The type of the value associated with the tag. Must be a non-reference, non-void type.
	///
	template <auto TagValue, typename V = std::monostate>
		requires(std::same_as<std::remove_cvref_t<V>, V> && !std::same_as<V, void>)
	struct Tag
	{
		V value;

		///
		/// @brief Tag value of the type
		///
		///
		static constexpr auto tag = TagValue;

		///
		/// @brief Type of the value associated with the tag
		///
		///
		using Type = V;
	};

	template <typename T>
	using TagVoidlessType = std::conditional_t<std::is_void_v<T>, std::monostate, T>;

	template <auto TagValue, typename V>
	Tag<TagValue, std::remove_cvref_t<V>> tag_value(V&& value) noexcept
	{
		return Tag<TagValue, std::remove_cvref_t<V>>{std::forward<V>(value)};
	}

	template <auto TagValue>
	Tag<TagValue> tag_value() noexcept
	{
		return Tag<TagValue>{std::monostate{}};
	}

	namespace impl
	{
		// Checks if all types in `Ts...` are the same as T
		template <typename T, typename... Ts>
		struct UniformEnum
		{
			// Check if all types in `Ts...` are the same as T
			static constexpr bool satisfied = (std::same_as<T, Ts> && ...);

			// The common type
			using Type = std::remove_cvref_t<T>;
		};

		template <typename T>
		constexpr bool is_tagged = false;

		template <auto TagValue, typename V>
		constexpr bool is_tagged<Tag<TagValue, V>> = true;

		// Check if all tags in `T...` are unique
		template <auto... T>
		consteval bool non_repeating_tags()
		{
			using EnumType = UniformEnum<decltype(T)...>::Type;

			std::array<EnumType, sizeof...(T)> tags{T...};
			return std::ranges::unique(tags).begin() == tags.end();
		}
	}

	///
	/// @brief Concept that requires all values in `Vs...` to be of the same type
	///
	template <auto... Vs>
	concept UniformType = (sizeof...(Vs) > 0) && impl::UniformEnum<decltype(Vs)...>::satisfied;

	///
	/// @brief Concept that requires all tags in `Vs...` to be unique
	///
	template <auto... Vs>
	concept UniqueTags = UniformType<Vs...> && impl::non_repeating_tags<Vs...>();

	///
	/// @brief The common type of the tags in `Vs...`
	///
	/// @tparam Vs
	///
	template <auto... Vs>
	using EnumType = impl::UniformEnum<decltype(Vs)...>::Type;

	///
	/// @brief Concept that requires a type to be a `Tag` type
	///
	template <typename T>
	concept TagType = impl::is_tagged<T>;

	///
	/// @brief Concept that requires the types to be valid for use in `EnumVariant`
	///
	template <typename... T>
	concept EnumVariantValid = (sizeof...(T) > 0) && (TagType<T> && ...) && UniqueTags<T::tag...>;

	namespace impl
	{
		// Deduces the index of the type in `Tags...` that corresponds to the given `Tag`
		template <auto Tag, TagType... Tags>
			requires(EnumVariantValid<Tags...> && std::same_as<EnumType<Tags::tag...>, decltype(Tag)>)
		consteval size_t deduce_type_index()
		{
			size_t index = 0;
			bool found = false;

			((Tags::tag == Tag ? (found = true) : (index += static_cast<size_t>(!found))), ...);
			if (!found) throw std::logic_error("Tag not found in Tags...");

			return index;
		}

		// Deduces the type associated with the given `Tag` in `Tags...`
		template <auto Tag, TagType... Tags>
			requires(EnumVariantValid<Tags...> && std::same_as<EnumType<Tags::tag...>, decltype(Tag)>)
		using DeducedType = std::remove_cvref_t<decltype(std::get<deduce_type_index<Tag, Tags...>()>(
			std::declval<std::variant<typename Tags::Type...>>()
		))>;
	}

	///
	/// @brief A variant type that holds a value associated with a compile-time tag. Serves a similar purpose
	/// to an `enum` in Rust.
	///
	/// @tparam Tags A list of `Tag` types that define the possible values and their associated tags.
	/// The tags must be unique and of the same type.
	///
	template <TagType... Tags>
		requires(EnumVariantValid<Tags...>)
	class EnumVariant
	{
	  protected:

		std::variant<Tags...> value;

		explicit constexpr EnumVariant(std::variant<Tags...> init) :
			value(std::move(init))
		{}

	  public:

		///
		/// @brief Type of the tags
		///
		using EnumType = util::EnumType<Tags::tag...>;

		///
		/// @brief Type of the value associated with the given `Tag`
		///
		template <EnumType TagValue>
		using DeducedType = impl::DeducedType<TagValue, Tags...>;

		///
		/// @brief Construct an `EnumVariant` from a tagged value directly
		///
		/// @tparam TagValue The tag associated with the value. Must be one of the tags defined in `Tags...`.
		/// @param tagged_value The tagged value to be stored in the variant.
		///
		template <EnumType TagValue>
		constexpr EnumVariant(Tag<TagValue, DeducedType<TagValue>> tagged_value) :
			value(std::move(tagged_value))
		{}

		///
		/// @brief Construct an `EnumVariant` from a value associated with a specific tag.
		///
		/// @tparam TagValue The tag associated with the value. Must be one of the tags defined in `Tags...`.
		/// @param value The value to be stored in the variant, associated with the specified tag. The type is
		/// automatically deduced from `Tag`
		///
		/// @return An `EnumVariant` instance containing the provided value associated with the
		/// specified tag.
		///
		template <EnumType TagValue>
		[[nodiscard]]
		static constexpr EnumVariant from(DeducedType<TagValue> value) noexcept
		{
			return EnumVariant(std::variant<Tags...>{Tag<TagValue, DeducedType<TagValue>>{std::move(value)}});
		}

		///
		/// @brief Construct an `EnumVariant` from a tag with no associated value (i.e., the value is
		/// `std::monostate`).
		///
		/// @tparam TagValue The tag to be set in the variant. Must be one of the tags defined in `Tags...`,
		/// and the associated value type must be `std::monostate`.
		///
		/// @return An `EnumVariant` instance containing the specified tag with an associated value of
		/// `std::monostate`.
		///
		template <EnumType TagValue>
			requires(std::same_as<DeducedType<TagValue>, std::monostate>)
		[[nodiscard]]
		static constexpr EnumVariant from() noexcept
		{
			return EnumVariant(std::variant<Tags...>{Tag<TagValue>{std::monostate()}});
		}

		///
		/// @brief Set the value associated with a specific tag in the variant
		///
		/// @tparam TagValue The tag associated with the value to be set. Must be one of the tags defined in
		/// `Tags...`.
		/// @param value The value to be set in the variant, associated with the specified tag. The type is
		/// automatically deduced from `Tag`
		///
		template <EnumType Tag>
		constexpr void set(DeducedType<Tag> value) noexcept
		{
			this->value.template emplace<impl::deduce_type_index<Tag, Tags...>()>(std::move(value));
		}

		///
		/// @brief Set the value associated with a specific tag in the variant to `std::monostate`
		///
		/// @tparam TagValue The tag associated with the value to be set. Must be one of the tags defined in
		/// `Tags...`, and the associated value type must be `std::monostate`.
		///
		template <EnumType Tag>
			requires(std::same_as<DeducedType<Tag>, std::monostate>)
		constexpr void set() noexcept
		{
			this->value.template emplace<impl::deduce_type_index<Tag, Tags...>()>(std::monostate{});
		}

		///
		/// @brief Get the value (optional) associated with a specific tag in the variant
		///
		/// @tparam TagValue The tag associated with the value to be retrieved. Must be one of the tags
		/// defined in `Tags...`.
		///
		/// @return An optional containing a reference to the value if the tag is currently active, or
		/// std::nullopt otherwise.
		///
		template <EnumType TagValue>
		[[nodiscard]]
		constexpr auto get_if(this auto&& self) noexcept -> std::optional<
			decltype(std::ref(std::get<util::Tag<TagValue, DeducedType<TagValue>>>(self.value).value))
		>
		{
			if (std::holds_alternative<util::Tag<TagValue, DeducedType<TagValue>>>(self.value))
				return std::make_optional(
					std::ref(std::get<util::Tag<TagValue, DeducedType<TagValue>>>(self.value).value)
				);
			return std::nullopt;
		}

		///
		/// @brief Get the value associated with a specific tag in the variant. Terminates if the tag is not
		/// currently active.
		/// @warning Only use if the caller can guarantee that the tag is active, otherwise use
		/// `get_if` and check the result.
		///
		/// @tparam TagValue The tag associated with the value to be retrieved. Must be one of the tags
		/// defined in `Tags...`.
		///
		/// @return A reference to the value associated with the specified tag if it is currently active.
		/// Terminates with `std::bad_variant_access` if the tag is not currently active.
		///
		template <EnumType TagValue>
			requires(!std::same_as<DeducedType<TagValue>, std::monostate>)
		[[nodiscard]]
		constexpr auto&& get(this auto&& self) noexcept
		{
			return std::get<util::Tag<TagValue, DeducedType<TagValue>>>(
					   std::forward<decltype(self)>(self).value
			)
				.value;
		}

		///
		/// @brief Get the tag of the currently active value in the variant
		///
		/// @return The tag of the currently active value in the variant.
		///
		[[nodiscard]]
		constexpr EnumType tag() const noexcept
		{
			return std::visit(
				[](const auto& val) { return std::remove_cvref_t<decltype(val)>::tag; },
				this->value
			);
		}

		///
		/// @brief Visit the underlying value in the variant with a visitor function.
		/// @note The visitor directly takes the `Tag` type as an argument
		///
		/// @param f The visitor function to be applied to the active value in the variant
		///
		[[nodiscard]]
		constexpr auto visit(this auto&& self, auto&& f)
		{
			return std::visit(std::forward<decltype(f)>(f), std::forward<decltype(self)>(self).value);
		}

		///
		/// @brief Get the index of the currently active value in the variant.
		///
		/// @return The index of the currently active value in the variant, corresponding to the order of
		/// `Tags...`.
		///
		[[nodiscard]]
		size_t index() const noexcept
		{
			return value.index();
		}

		///
		/// @brief Get the number of tags defined in the variant type
		///
		/// @return The number of tags defined in the variant type, which corresponds to the number of
		/// possible values
		///
		[[nodiscard]]
		constexpr size_t tag_count() const noexcept
		{
			return sizeof...(Tags);
		}

		EnumVariant(const EnumVariant&)
			requires(std::copy_constructible<std::variant<Tags...>>)
		= default;
		EnumVariant(const EnumVariant&) = delete;

		EnumVariant& operator=(const EnumVariant&)
			requires(std::copy_constructible<std::variant<Tags...>>)
		= default;
		EnumVariant& operator=(const EnumVariant&) = delete;

		EnumVariant(EnumVariant&&) = default;
		EnumVariant& operator=(EnumVariant&&) = default;
	};

	///
	/// @brief A internally synced variant type that holds a value associated with a compile-time tag. Serves
	/// a similar purpose to an `enum` in Rust.
	///
	/// @tparam Tags A list of `Tag` types that define the possible values and their associated tags.
	/// The tags must be unique and of the same type.
	///
	template <TagType... Tags>
		requires(EnumVariantValid<Tags...>)
	class SyncedEnumVariant : public EnumVariant<Tags...>
	{
		std::shared_ptr<std::mutex> mutex;

	  public:

		///
		/// @brief Type of the tags
		///
		using EnumType = util::EnumType<Tags::tag...>;

		///
		/// @brief Type of the value associated with the given `Tag`
		///
		template <EnumType TagValue>
		using DeducedType = impl::DeducedType<TagValue, Tags...>;

		///
		/// @brief Construct a new `SyncedEnumVariant` from an existing `EnumVariant`. The new
		/// `SyncedEnumVariant` will have the same active tag and value as the original `EnumVariant`, but
		/// with an independent mutex for synchronization.
		///
		/// @param other The `EnumVariant` to copy the active tag and value from.
		///
		explicit SyncedEnumVariant(const EnumVariant<Tags...>& other)
			requires(std::copy_constructible<std::variant<Tags...>>)
			:
			EnumVariant<Tags...>(other),
			mutex(std::make_shared<std::mutex>())
		{}

		///
		/// @brief Construct a new `SyncedEnumVariant` from an existing `EnumVariant`. The new
		/// `SyncedEnumVariant` will have the same active tag and value as the original `EnumVariant`, but
		/// with an independent mutex for synchronization.
		///
		/// @param other The `EnumVariant` to copy the active tag and value from.
		///
		explicit SyncedEnumVariant(EnumVariant<Tags...>&& other) :
			EnumVariant<Tags...>(std::move(other)),
			mutex(std::make_shared<std::mutex>())
		{}

		///
		/// @brief Construct an `SyncedEnumVariant` from a tagged value directly
		///
		/// @tparam TagValue The tag associated with the value. Must be one of the tags defined in `Tags...`.
		/// @param tagged_value The tagged value to be stored in the variant.
		/// @note This constructor is thread-safe.
		///
		template <EnumType TagValue>
		SyncedEnumVariant(Tag<TagValue, DeducedType<TagValue>> tagged_value) :
			EnumVariant<Tags...>(std::move(tagged_value)),
			mutex(std::make_shared<std::mutex>())
		{}

		///
		/// @brief Construct an `SyncedEnumVariant` from a value associated with a specific tag.
		///
		/// @tparam TagValue The tag associated with the value. Must be one of the tags defined in `Tags...`.
		/// @param value The value to be stored in the variant, associated with the specified tag. The type is
		/// automatically deduced from `Tag`
		///
		/// @return An `SyncedEnumVariant` instance containing the provided value associated with the
		/// specified tag.
		/// @note This function is thread-safe.
		///
		template <EnumType TagValue>
		[[nodiscard]]
		static SyncedEnumVariant from(DeducedType<TagValue> value) noexcept
		{
			return SyncedEnumVariant(Tag<TagValue, DeducedType<TagValue>>{std::move(value)});
		}

		///
		/// @brief Construct an `SyncedEnumVariant` from a tag with no associated value (i.e., the value is
		/// `std::monostate`).
		///
		/// @tparam TagValue The tag to be set in the variant. Must be one of the tags defined in `Tags...`,
		/// and the associated value type must be `std::monostate`.
		///
		/// @return An `SyncedEnumVariant` instance containing the specified tag with an associated value of
		/// `std::monostate`.
		/// @note This function is thread-safe.
		///
		template <EnumType TagValue>
			requires(std::same_as<DeducedType<TagValue>, std::monostate>)
		[[nodiscard]]
		static SyncedEnumVariant from() noexcept
		{
			return SyncedEnumVariant(Tag<TagValue>{std::monostate{}});
		}

		///
		/// @brief Set the value associated with a specific tag in the variant
		///
		/// @tparam TagValue The tag associated with the value to be set. Must be one of the tags defined in
		/// `Tags...`.
		/// @param value The value to be set in the variant, associated with the specified tag. The type is
		/// automatically deduced from `Tag`
		/// @note This function is thread-safe.
		///
		template <EnumType Tag>
		void set(DeducedType<Tag> value) noexcept
		{
			const std::scoped_lock lock(*mutex);
			EnumVariant<Tags...>::template set<Tag>(std::move(value));
		}

		///
		/// @brief Set the value associated with a specific tag in the variant to `std::monostate`
		///
		/// @tparam TagValue The tag associated with the value to be set. Must be one of the tags defined in
		/// `Tags...`, and the associated value type must be `std::monostate`.
		/// @note This function is thread-safe.
		///
		template <EnumType Tag>
			requires(std::same_as<DeducedType<Tag>, std::monostate>)
		void set() noexcept
		{
			const std::scoped_lock lock(*mutex);
			EnumVariant<Tags...>::template set<Tag>();
		}

		///
		/// @brief Get the value (optional) associated with a specific tag in the variant
		///
		/// @tparam TagValue The tag associated with the value to be retrieved. Must be one of the tags
		/// defined in `Tags...`.
		///
		/// @return An optional containing a reference to the value if the tag is currently active, or
		/// std::nullopt otherwise.
		/// @note This function is thread-safe.
		///
		template <EnumType TagValue>
		[[nodiscard]]
		auto get_if(this auto&& self) noexcept -> std::optional<
			decltype(std::ref(std::declval<util::Tag<TagValue, DeducedType<TagValue>>>().value))
		>
		{
			const std::scoped_lock lock(*self.mutex);
			return self.EnumVariant<Tags...>::template get_if<TagValue>();
		}

		///
		/// @brief Get the value associated with a specific tag in the variant. Terminates if the tag is not
		/// currently active.
		/// @warning Only use if the caller can guarantee that the tag is active, otherwise use
		/// `get_if` and check the result.
		///
		/// @tparam TagValue The tag associated with the value to be retrieved. Must be one of the tags
		/// defined in `Tags...`.
		///
		/// @return A reference to the value associated with the specified tag if it is currently active.
		/// Terminates with `std::bad_variant_access` if the tag is not currently active.
		/// @note This function is thread-safe.
		///
		template <EnumType TagValue>
			requires(!std::same_as<DeducedType<TagValue>, std::monostate>)
		[[nodiscard]]
		auto&& get(this auto&& self) noexcept
		{
			const std::scoped_lock lock(*self.mutex);
			return self.EnumVariant<Tags...>::template get<TagValue>();
		}

		///
		/// @brief Get the tag of the currently active value in the variant
		///
		/// @return The tag of the currently active value in the variant.
		/// @note This function is thread-safe.
		///
		[[nodiscard]]
		EnumType tag() const noexcept
		{
			const std::scoped_lock lock(*mutex);
			return EnumVariant<Tags...>::tag();
		}

		///
		/// @brief Visit the underlying value in the variant with a visitor function.
		/// @note The visitor directly takes the `Tag` type as an argument
		///
		/// @param f The visitor function to be applied to the active value in the variant
		/// @note This function is thread-safe.
		///
		[[nodiscard]]
		auto visit(this auto&& self, auto&& f)
		{
			const std::scoped_lock lock(*self.mutex);
			return self.EnumVariant<Tags...>::visit(std::forward<decltype(f)>(f));
		}

		///
		/// @brief Convert the `SyncedEnumVariant` to a shared pointer. This allows multiple threads to share
		/// ownership of the same `SyncedEnumVariant` instance
		///
		/// @return A `std::shared_ptr` to the current `SyncedEnumVariant` instance
		///
		std::shared_ptr<SyncedEnumVariant> share() && noexcept
		{
			return std::make_shared<SyncedEnumVariant>(std::move(*this));
		}

		SyncedEnumVariant(const SyncedEnumVariant&) = delete;
		SyncedEnumVariant& operator=(const SyncedEnumVariant&) = delete;
		SyncedEnumVariant(SyncedEnumVariant&&) = default;
		SyncedEnumVariant& operator=(SyncedEnumVariant&&) = default;
	};
}

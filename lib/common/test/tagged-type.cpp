#include "common/util/tagged-type.hpp"
#include "common/util/overload.hpp"

#include <doctest.h>
#include <memory>

TEST_CASE("EnumVariant basic")
{
	enum class MyEnum
	{
		A,
		B,
		C
	};

	struct Test
	{
		int a, b;
	};

	using MyTaggedVariant = util::
		EnumVariant<util::Tag<MyEnum::A, int>, util::Tag<MyEnum::B, Test>, util::Tag<MyEnum::C, double>>;

	const auto a = MyTaggedVariant::from<MyEnum::A>(42);
	const auto b = MyTaggedVariant::from<MyEnum::B>({.a = 42, .b = 100});
	const auto c = MyTaggedVariant::from<MyEnum::C>(3.14);

	SUBCASE("tags")
	{
		CHECK_EQ(a.tag(), MyEnum::A);
		CHECK_EQ(b.tag(), MyEnum::B);
		CHECK_EQ(c.tag(), MyEnum::C);
	}

	SUBCASE("get")
	{
		{
			const auto val = a.get_if<MyEnum::A>();
			REQUIRE(val.has_value());
			CHECK_EQ(val->get(), 42);
		}

		{
			const auto val = b.get_if<MyEnum::B>();
			REQUIRE(val.has_value());
			CHECK_EQ(val->get().a, 42);
			CHECK_EQ(val->get().b, 100);
		}

		{
			const auto val = c.get_if<MyEnum::C>();
			REQUIRE(val.has_value());
			CHECK_EQ(val->get(), 3.14);
		}
	}

	SUBCASE("visit with type checks")
	{
		const auto visit1 = a.visit([](const auto& val) {
			if constexpr (std::same_as<std::remove_cvref_t<decltype(val)>, util::Tag<MyEnum::A, int>>)
				return 0;
			else if constexpr (std::same_as<std::remove_cvref_t<decltype(val)>, util::Tag<MyEnum::B, Test>>)
				return 1;
			else if constexpr (std::same_as<std::remove_cvref_t<decltype(val)>, util::Tag<MyEnum::C, double>>)
				return 2;
			else
				static_assert(false, "Non-exhaustive visitor for EnumVariant");
		});

		CHECK_EQ(visit1, 0);

		const auto visit2 = a.visit([]<auto e, typename T>(const util::Tag<e, T>&) { return e; });
		CHECK_EQ(visit2, MyEnum::A);
	}
}

TEST_CASE("EnumVariant move-only type")
{
	enum class E
	{
		X,
		Y
	};

	using MoveTagged = util::EnumVariant<util::Tag<E::X, std::unique_ptr<int>>, util::Tag<E::Y, int>>;

	auto ptr = std::make_unique<int>(7);
	auto v = MoveTagged::from<E::X>(std::move(ptr));

	CHECK_EQ(v.tag(), E::X);
	{
		const auto val = v.get_if<E::X>();
		REQUIRE(val.has_value());
		CHECK_EQ(*val->get(), 7);
	}

	auto moved = std::exchange(v, MoveTagged::from<E::Y>(42));
	CHECK_EQ(moved.tag(), E::X);
	{
		const auto val = moved.get_if<E::X>();
		REQUIRE(val.has_value());
		CHECK_EQ(*val->get(), 7);
	}

	CHECK_EQ(v.tag(), E::Y);
	{
		const auto val = v.get_if<E::Y>();
		REQUIRE(val.has_value());
		CHECK_EQ(val->get(), 42);
	}

	std::swap(v, moved);
	CHECK_EQ(moved.tag(), E::Y);
	{
		const auto val = moved.get_if<E::Y>();
		REQUIRE(val.has_value());
		CHECK_EQ(val->get(), 42);
	}

	CHECK_EQ(v.tag(), E::X);
	{
		const auto val = v.get_if<E::X>();
		REQUIRE(val.has_value());
		CHECK_EQ(*val->get(), 7);
	}
}

#include "vulkan/util/cycle.hpp"
#include <doctest.h>

namespace
{
	struct MoveCounter
	{
		int value;
		int time_moved = 0;

		MoveCounter(int value) :
			value(value)
		{}

		MoveCounter(const MoveCounter&) = delete;
		MoveCounter& operator=(const MoveCounter&) = delete;

		MoveCounter(MoveCounter&& other) noexcept :
			value(other.value),
			time_moved(other.time_moved + 1)
		{}

		MoveCounter& operator=(MoveCounter&& other) noexcept
		{
			value = other.value;
			time_moved = other.time_moved + 1;
			return *this;
		}
	};
}

TEST_CASE("Construction")
{
	SUBCASE("From vector")
	{
		const std::vector<int> items = {1, 2, 3};
		const auto cycle = vulkan::Cycle(items);

		CHECK(cycle.current() == 3);
		CHECK(cycle.prev() == 1);
	}

	SUBCASE("From array")
	{
		const std::array<int, 3> items = {1, 2, 3};
		const auto cycle = vulkan::Cycle(items);

		CHECK(cycle.current() == 3);
		CHECK(cycle.prev() == 1);
	}

	SUBCASE("From range")
	{
		const auto cycle = vulkan::Cycle(std::views::iota(1, 4));

		CHECK(cycle.current() == 3);
		CHECK(cycle.prev() == 1);
	}

	SUBCASE("From transformed range")
	{
		const auto cycle =
			vulkan::Cycle(std::views::iota(1, 4) | std::views::transform([](int x) { return x * 2; }));

		CHECK(cycle.current() == 6);
		CHECK(cycle.prev() == 2);
	}

	SUBCASE("Move only type")
	{
		std::vector<MoveCounter> items;
		items.reserve(3);
		items.emplace_back(1);
		items.emplace_back(2);
		items.emplace_back(3);

		const auto cycle = vulkan::Cycle(std::move(items));

		CHECK(cycle.current().value == 3);
		CHECK(cycle.prev().value == 1);

		CHECK(cycle.current().time_moved == 1);
	}
}

TEST_CASE("Cycle")
{
	const std::vector<int> items = {1, 2, 3};
	auto cycle = vulkan::Cycle(items);

	// [1, 2, 3]
	CHECK(cycle.current() == 3);
	CHECK(cycle.prev() == 1);

	cycle.cycle();

	// [3, 1, 2]
	CHECK(cycle.current() == 2);
	CHECK(cycle.prev() == 3);

	cycle.cycle();

	// [2, 3, 1]
	CHECK(cycle.current() == 1);
	CHECK(cycle.prev() == 2);
}

TEST_CASE("Iterate")
{
	SUBCASE("Iterate through items")
	{
		const std::vector<int> items = {1, 2, 3};
		const auto cycle = vulkan::Cycle(items);

		const auto iterated_items = cycle.iterate() | std::ranges::to<std::vector>();

		REQUIRE(iterated_items.size() == 3);
		CHECK(iterated_items[0] == 1);
		CHECK(iterated_items[1] == 2);
		CHECK(iterated_items[2] == 3);
	}

	SUBCASE("Iterate through pairs")
	{
		const std::vector<int> items = {1, 2, 3};
		const auto cycle = vulkan::Cycle(items);

		const auto iterated_pairs = cycle.iterate_pair() | std::ranges::to<std::vector>();

		REQUIRE(iterated_pairs.size() == 3);

		CHECK(iterated_pairs[0].first == 2);
		CHECK(iterated_pairs[0].second == 1);

		CHECK(iterated_pairs[1].first == 3);
		CHECK(iterated_pairs[1].second == 2);

		CHECK(iterated_pairs[2].first == 1);
		CHECK(iterated_pairs[2].second == 3);
	}
}

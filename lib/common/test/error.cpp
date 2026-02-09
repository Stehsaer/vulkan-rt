#include "common/util/error.hpp"

#include <doctest.h>
#include <source_location>

TEST_CASE("Error creation")
{
	SUBCASE("Without detail")
	{
		const auto loc = std::source_location::current();
		const auto err = Error("Test error", loc);

		CHECK_EQ(err.message, "Test error");
		CHECK_EQ(err.detail, std::nullopt);
		CHECK_EQ(err.location.file_name(), loc.file_name());
		CHECK_EQ(err.location.line(), loc.line());
		CHECK_EQ(err.location.column(), loc.column());
		CHECK_EQ(err.location.function_name(), loc.function_name());
	}

	SUBCASE("With detail")
	{
		const auto loc = std::source_location::current();
		const auto err = Error("Test error", "This is a test error", loc);

		CHECK_EQ(err.message, "Test error");
		CHECK_EQ(err.detail, "This is a test error");
		CHECK_EQ(err.location.file_name(), loc.file_name());
		CHECK_EQ(err.location.line(), loc.line());
		CHECK_EQ(err.location.column(), loc.column());
		CHECK_EQ(err.location.function_name(), loc.function_name());
	}
}

struct FooStruct
{};

template <>
Error Error::FromFunctor<FooStruct>::operator()(const FooStruct& foo [[maybe_unused]]) const noexcept
{
	return Error("FooStruct error");
}

TEST_CASE("Error::from")
{
	SUBCASE("vk::Result")
	{
		const vk::Result vk_res = vk::Result::eSuccess;
		const auto err = Error::from<vk::Result>(vk_res);
		CHECK_EQ(err.message, "Vulkan operation failed");
		CHECK_EQ(err.detail, vk::to_string(vk_res));

		const auto monaded =
			std::expected<int, vk::Result>(std::unexpected(vk_res))
				.transform_error(Error::from<vk::Result>());
		REQUIRE(!monaded);
		CHECK_EQ(monaded.error().message, "Vulkan operation failed");
		CHECK_EQ(monaded.error().detail, vk::to_string(vk_res));
	}

	SUBCASE("Custom type specialization")
	{
		const FooStruct foo;
		const auto err = Error::from<FooStruct>(foo);

		CHECK_EQ(err.message, "FooStruct error");
	}
}

TEST_CASE("Error formatting")
{
	const auto loc = std::source_location::current();
	const auto err = Error("Test error", "This is a test error", loc);

	REQUIRE(err.detail.has_value());

	CHECK_EQ(
		std::format("{}", err),
		std::format(
			"({}:{}) {}: {}",
			err.location.file_name(),
			err.location.line(),
			err.message,
			err.detail.value_or("")
		)
	);
	CHECK_EQ(std::format("{:message}", err), "Test error");
	CHECK_EQ(std::format("{:detail}", err), "This is a test error");
	CHECK_EQ(std::format("{:line}", err), std::to_string(err.location.line()));
	CHECK_EQ(std::format("{:file}", err), err.location.file_name());
	CHECK_EQ(std::format("{:func}", err), err.location.function_name());
	CHECK_EQ(std::format("{:loc}", err), std::format("{}:{}", err.location.file_name(), err.location.line()));
}

TEST_CASE("Error forwarding")
{
	const auto origin = Error("Origin error", "This is the origin error");
	const auto forwarded = origin.forward("Forwarded error", "This is the forwarded error");

	CHECK_EQ(forwarded.message, "Forwarded error");
	CHECK_EQ(forwarded.detail, "This is the forwarded error");
	REQUIRE_NE(forwarded.cause.get(), nullptr);

	const auto& cause = *forwarded.cause;
	CHECK_EQ(cause.message, "Origin error");
	CHECK_EQ(cause.detail, "This is the origin error");
}

TEST_CASE("Error unwrapping")
{
	SUBCASE("Negative")
	{
		const std::expected<void, Error> test = Error("Test error");
		CHECK_THROWS_AS(std::move(test) | Error::unwrap(), const Error&);
	}

	SUBCASE("Positive")
	{
		const std::expected<int, Error> test = 10;
		CHECK_EQ(std::move(test) | Error::unwrap(), 10);
	}
}

TEST_CASE("Error chaining")
{
	const auto error = Error("Level 0").forward("Level 1").forward("Level 2");

	SUBCASE("For iterator")
	{
		std::vector<std::string> messages;
		for (const auto& err : error.chain()) messages.push_back(err.message);

		REQUIRE_EQ(messages.size(), 3);
		CHECK_EQ(messages[0], "Level 2");
		CHECK_EQ(messages[1], "Level 1");
		CHECK_EQ(messages[2], "Level 0");
	}

	SUBCASE("std::views::enumerate")
	{
		for (const auto& [idx, err] : std::views::enumerate(error.chain()))
		{
			switch (idx)
			{
			case 0:
				CHECK_EQ(err.message, "Level 2");
				break;
			case 1:
				CHECK_EQ(err.message, "Level 1");
				break;
			case 2:
				CHECK_EQ(err.message, "Level 0");
				break;
			default:
				FAIL("Too many errors in chain");
			}
		}
	}

	SUBCASE("std::ranges::to")
	{
		const auto messages =
			error.chain() | std::views::transform(&Error::message) | std::ranges::to<std::vector>();
		REQUIRE_EQ(messages.size(), 3);
		CHECK_EQ(messages[0], "Level 2");
		CHECK_EQ(messages[1], "Level 1");
		CHECK_EQ(messages[2], "Level 0");
	}
}

TEST_CASE("Error collecting")
{
	SUBCASE("All success")
	{
		const std::vector<std::expected<int, Error>> vec = {10, 20, 30};
		const auto collected = vec | Error::collect_vec();

		REQUIRE(collected);
		REQUIRE_EQ(collected->size(), 3);
		CHECK_EQ((*collected)[0], 10);
		CHECK_EQ((*collected)[1], 20);
		CHECK_EQ((*collected)[2], 30);
	}

	SUBCASE("With error")
	{
		const std::vector<std::expected<int, Error>> vec = {10, Error("Second element error"), 30};
		const auto collected = vec | Error::collect_vec();

		REQUIRE(!collected);
		CHECK_EQ(collected.error().message, "Error in vector element");
		CHECK_EQ(collected.error().detail, "Error found in index 1");

		REQUIRE(collected.error().cause);
		CHECK_EQ(collected.error().cause->message, "Second element error");
	}
}
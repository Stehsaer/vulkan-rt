#include "common/file.hpp"
#include "common/util/span.hpp"

#include <algorithm>
#include <doctest.h>
#include <filesystem>
#include <print>
#include <random>

TEST_CASE("Write and read")
{
	/* Generate Random Data */

	std::mt19937_64 rng(114514);
	std::uniform_int_distribution<uint16_t> dist(0, std::numeric_limits<uint8_t>::max());

	std::vector<uint8_t> random_data;
	random_data.reserve(8192);
	std::generate_n(std::back_inserter(random_data), 8192, [&rng, &dist]() { return dist(rng); });

	/* Generate Paths */

	const auto env = std::getenv("TEMP_DIR");
	REQUIRE(env != nullptr);

	const auto temp_dir = std::filesystem::path(env);
	REQUIRE(std::filesystem::exists(temp_dir));
	const auto temp_file = temp_dir / "lib.common-file-test.bin";

	/* Write first 4096 bytes */

	const auto write_result =
		file::write(temp_file, util::as_bytes(random_data).subspan(0, 4096), file::WriteMode::Overwrite);
	REQUIRE(write_result);

	/* Read back 4096 bytes */

	const auto read_result = file::read(temp_file);
	REQUIRE(read_result);
	REQUIRE(read_result->size() == 4096);
	CHECK(std::ranges::equal(util::as_bytes(random_data).subspan(0, 4096), util::as_bytes(*read_result)));

	/* Try reading with 1024 bytes size limit */

	const auto limited_read_result = file::read(temp_file, 1024);
	REQUIRE_FALSE(limited_read_result);

	/* Append another 4096 bytes */

	const auto append_result =
		file::write(temp_file, util::as_bytes(random_data).subspan(4096), file::WriteMode::Append);
	REQUIRE(append_result);

	/* Read back all 8192 bytes */

	const auto read_appended_result = file::read(temp_file);
	REQUIRE(read_appended_result);
	REQUIRE(read_appended_result->size() == 8192);
	CHECK(std::ranges::equal(util::as_bytes(random_data), util::as_bytes(*read_appended_result)));

	REQUIRE(std::filesystem::remove(temp_file));
}

TEST_CASE("Read non-existent file")
{
	const auto env = std::getenv("TEMP_DIR");
	REQUIRE(env != nullptr);

	const auto temp_dir = std::filesystem::path(env);
	REQUIRE(std::filesystem::exists(temp_dir));
	const auto non_existent_file = temp_dir / "lib.common-file-test-non-existent.bin";

	if (std::filesystem::exists(non_existent_file))
	{
		std::println("Warning: file exists. Deleting it for test.");
		std::filesystem::remove(non_existent_file);
	}

	const auto read_result = file::read(non_existent_file);
	REQUIRE_FALSE(read_result);
}

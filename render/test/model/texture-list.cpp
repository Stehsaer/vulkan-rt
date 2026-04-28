#include <coro/thread_pool.hpp>
#include <doctest.h>
#include <vulkan/vulkan.hpp>

#include "common/file.hpp"
#include "common/test-macro.hpp"
#include "image/common.hpp"
#include "image/image.hpp"
#include "test-common.hpp"
#include "render/model/texture-list.hpp"
#include "vulkan/test-driver.hpp"
#include "vulkan/util/resource-readback.hpp"

TEST_CASE("Normal")
{
	const auto material_list = test::create_material_list();

	auto texture_list_result = [&material_list] {
		auto thread_pool = coro::thread_pool::make_unique();
		const auto load_option = render::TextureList::LoadOption{
			.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
			.max_pending_data_size = 4 * 1048576
		};

		return coro::sync_wait(
			render::TextureList::create(
				*thread_pool,
				util::Progress(),
				vulkan::get_test_context().get(),
				material_list,
				load_option
			)
		);
	}();

	EXPECT_SUCCESS(texture_list_result);
	auto texture_list = std::move(*texture_list_result);

	// test::check_texture_list(texture_list);

	const auto os_tempdir = std::getenv("TEMP_DIR");
	REQUIRE(os_tempdir != nullptr);
	const auto path = std::filesystem::path(os_tempdir);

	// Readback error texture
	const auto write_error_result =
		vulkan::readback_image<image::Format::Unorm8, image::Layout::RGBA>(
			vulkan::get_test_context().get(),
			texture_list.get_color_texture(65).texture.image,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::Format::eR8G8B8A8Unorm,
			{16, 16}
		)
			.and_then([](const image::Image<image::Format::Unorm8, image::Layout::RGBA>& image) {
				return image.encode(image::encode_format::Png());
			})
			.and_then([&path](std::span<const std::byte> data) {
				return file::write(path / "error-hint.png", data);
			});
	EXPECT_SUCCESS(write_error_result);

	// readback valid texture
	const auto write_valid_result =
		vulkan::readback_image<image::Format::Unorm8, image::Layout::RGBA>(
			vulkan::get_test_context().get(),
			texture_list.get_color_texture(0).texture.image,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::Format::eR8G8B8A8Unorm,
			{128, 128}
		)
			.and_then([](const image::Image<image::Format::Unorm8, image::Layout::RGBA>& image) {
				return image.encode(image::encode_format::Png());
			})
			.and_then([&path](std::span<const std::byte> data) {
				return file::write(path / "valid-texture.png", data);
			});
	EXPECT_SUCCESS(write_valid_result);
}

TEST_CASE("Exit on fail")
{
	const auto material_list = test::create_erroneous_material_list();

	auto texture_list_result = [&material_list] {
		auto thread_pool = coro::thread_pool::make_unique();
		const auto load_option = render::TextureList::LoadOption{
			.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
			.exit_on_failed_load = true,
			.max_pending_data_size = 4 * 1048576
		};

		return coro::sync_wait(
			render::TextureList::create(
				*thread_pool,
				util::Progress(),
				vulkan::get_test_context().get(),
				material_list,
				load_option
			)
		);
	}();

	EXPECT_FAIL(texture_list_result);
}

TEST_CASE("No exit on fail")
{
	const auto material_list = test::create_erroneous_material_list();

	auto texture_list_result = [&material_list] {
		auto thread_pool = coro::thread_pool::make_unique();
		const auto load_option = render::TextureList::LoadOption{
			.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferSrc,
			.exit_on_failed_load = false,
			.max_pending_data_size = 4 * 1048576
		};

		return coro::sync_wait(
			render::TextureList::create(
				*thread_pool,
				util::Progress(),
				vulkan::get_test_context().get(),
				material_list,
				load_option
			)
		);
	}();

	EXPECT_SUCCESS(texture_list_result);
}

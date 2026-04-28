#include <coro/sync_wait.hpp>
#include <coro/thread_pool.hpp>
#include <doctest.h>
#include <vulkan/vulkan.hpp>

#include "common/test-macro.hpp"
#include "render/model/material.hpp"
#include "test-common.hpp"
#include "vulkan/test-driver.hpp"

TEST_CASE("Normal")
{
	const auto material = test::create_material_list();

	auto material_layout_result = render::MaterialLayout::create(vulkan::get_test_context().get().device);
	EXPECT_SUCCESS(material_layout_result);
	auto layout = std::move(*material_layout_result);

	auto thread_pool = coro::thread_pool::make_unique();
	auto [material_task, material_progress] = render::MaterialList::create(
		*thread_pool,
		vulkan::get_test_context().get(),
		layout,
		material,
		render::TextureList::LoadOption{}
	);
	auto material_list_result = coro::sync_wait(std::move(material_task));
	EXPECT_SUCCESS(material_list_result);
}

TEST_CASE("Errneous material")
{
	/* NOTE: Exit on fail */

	const auto material = test::create_erroneous_material_list();

	auto material_layout_result = render::MaterialLayout::create(vulkan::get_test_context().get().device);
	EXPECT_SUCCESS(material_layout_result);
	auto layout = std::move(*material_layout_result);

	auto thread_pool = coro::thread_pool::make_unique();
	auto [material_task, material_progress] = render::MaterialList::create(
		*thread_pool,
		vulkan::get_test_context().get(),
		layout,
		material,
		render::TextureList::LoadOption{.exit_on_failed_load = true}
	);
	auto material_list_result = coro::sync_wait(std::move(material_task));
	EXPECT_FAIL(material_list_result);
}

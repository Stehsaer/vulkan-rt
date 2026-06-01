#include "render/model/model.hpp"
#include "common/util/error.hpp"
#include "model/model.hpp"
#include "render/model/blas.hpp"
#include "render/model/material.hpp"
#include "render/model/mesh.hpp"
#include "vulkan/interface/context.hpp"

#include <coro/task.hpp>
#include <coro/thread_pool.hpp>
#include <expected>
#include <memory>
#include <utility>

namespace render
{
	coro::task<std::expected<MeshList, Error>> Model::create_mesh(
		coro::thread_pool& thread_pool,
		const vulkan::Context& context,
		const model::Model& model
	) noexcept
	{
		co_await thread_pool.schedule();
		auto mesh_list = MeshList::create(context, model.meshes);
		co_return mesh_list;
	}

	coro::task<std::expected<BlasList, Error>> Model::create_blas(
		coro::thread_pool& thread_pool,
		const vulkan::Context& context,
		const MaterialList& material_list,
		const MeshList& mesh_list
	) noexcept
	{
		co_await thread_pool.schedule();
		auto blas_list = BlasList::create(context, mesh_list, material_list);
		co_return blas_list;
	}

	std::pair<coro::task<std::expected<Model, Error>>, std::shared_ptr<const Model::Progress>> Model::create(
		coro::thread_pool& thread_pool,
		const vulkan::Context& context,
		const MaterialLayout& material_layout,
		const model::Model& model,
		Option option
	) noexcept
	{
		auto progress = std::make_shared<Progress>(Progress::from<ProgressState::Preparing>());
		auto task = create_impl(thread_pool, progress, context, material_layout, model, option);
		return {std::move(task), progress};
	}

	coro::task<std::expected<Model, Error>> Model::create_impl(
		coro::thread_pool& thread_pool,
		std::shared_ptr<Progress> progress,
		const vulkan::Context& context,
		const MaterialLayout& material_layout,
		const model::Model& model,
		Option option
	) noexcept
	{
		auto [material_task, material_progress] = MaterialList::create(
			thread_pool,
			context,
			material_layout,
			model.material_list,
			option.texture_load_option
		);
		progress->set<ProgressState::Material>(material_progress);
		auto material_result = co_await std::move(material_task);

		progress->set<ProgressState::Mesh>();
		auto mesh_result = co_await create_mesh(thread_pool, context, model);

		if (!material_result) co_return material_result.error().forward("Create material list failed");
		if (!mesh_result) co_return mesh_result.error().forward("Create mesh list failed");

		auto material = std::move(*material_result);
		auto mesh = std::move(*mesh_result);

		progress->set<ProgressState::Blas>();
		auto blas_result = co_await create_blas(thread_pool, context, material, mesh);
		if (!blas_result) co_return blas_result.error().forward("Create BLAS failed");
		auto blas = std::move(*blas_result);

		co_return Model(model.hierarchy, std::move(mesh), std::move(material), std::move(blas));
	}
}

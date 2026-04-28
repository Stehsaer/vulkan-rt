#include "render/model/model.hpp"

namespace render
{
	coro::task<std::expected<MeshList, Error>> Model::create_mesh(
		coro::thread_pool& thread_pool,
		const vulkan::DeviceContext& context,
		const model::Model& model
	) noexcept
	{
		co_await thread_pool.schedule();
		auto mesh_list = MeshList::create(context, model.meshes);
		co_return mesh_list;
	}

	std::pair<coro::task<std::expected<Model, Error>>, std::shared_ptr<const Model::Progress>> Model::create(
		coro::thread_pool& thread_pool,
		const vulkan::DeviceContext& context,
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
		const vulkan::DeviceContext& context,
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

		co_return Model(model.hierarchy, std::move(*mesh_result), std::move(*material_result));
	}
}

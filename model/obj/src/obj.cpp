#include "model/obj.hpp"
#include "common/util/async.hpp"
#include "common/util/error.hpp"

#include "hierarchy.hpp"
#include "material.hpp"
#include "mesh.hpp"

#include <algorithm>
#include <filesystem>
#include <functional>
#include <ranges>
#include <tiny_obj_loader.h>
#include <utility>

namespace model::obj
{
	namespace
	{
		[[nodiscard]]
		coro::task<std::expected<Model, Error>> load_from_parsed_data(
			coro::thread_pool& thread_pool,
			Progress& progress,
			const tinyobj::ObjReader& reader,
			const std::function<TextureLoader>& texture_loader
		) noexcept
		{
			const auto& attributes = reader.GetAttrib();
			const auto& shapes = reader.GetShapes();
			const auto& materials = reader.GetMaterials();

			/*===== Convert meshes =====*/

			const auto non_empty_shape = [](const tinyobj::shape_t& shape) {
				return !shape.mesh.indices.empty() && !shape.mesh.num_face_vertices.empty();
			};

			auto mesh_progress = ::util::Progress();
			mesh_progress.set_total(std::ranges::count_if(shapes, non_empty_shape));
			progress.set<ProgressState::ProcessingMesh>(mesh_progress.get_ref());

			auto mesh_tasks =
				shapes
				| std::views::filter(non_empty_shape)
				| std::views::transform([](const tinyobj::shape_t& shape) { return std::cref(shape.mesh); })
				| std::views::transform(
					[&attributes, &thread_pool, &mesh_progress](const tinyobj::mesh_t& mesh) {
						return impl::convert_mesh_async(thread_pool, mesh_progress, attributes, mesh);
					}
				)
				| std::ranges::to<std::vector>();

			auto meshes_result = co_await coro::when_all(std::move(mesh_tasks))
				| std::views::transform([](auto&& result) { return result.return_value(); })
				| Error::collect();
			if (!meshes_result) co_return meshes_result.error().forward("Convert meshes failed");
			auto meshes = std::move(*meshes_result);

			/*===== Convert materials =====*/

			progress.set<ProgressState::ProcessingTextures>({});

			auto material_list_result = impl::convert_materials(texture_loader, materials);
			if (!material_list_result)
				co_return material_list_result.error().forward("Create material set failed");
			auto material_list = std::move(*material_list_result);

			/*===== Create hierarchy =====*/

			auto hierarchy = impl::create_hierarchy(meshes.size());

			/*===== Assemble model =====*/

			auto model_result =
				Model::assemble(std::move(material_list), std::move(meshes), std::move(hierarchy));
			if (!model_result) co_return model_result.error().forward("Assemble model failed");

			co_return std::move(*model_result);
		}

		[[nodiscard]]
		coro::task<std::expected<Model, Error>> load_from_path_impl(
			coro::thread_pool& thread_pool,
			std::shared_ptr<Progress> progress,
			std::filesystem::path file_path
		) noexcept
		{
			co_await thread_pool.schedule();

			/* Check path */

			try
			{
				if (!std::filesystem::exists(file_path)) co_return Error("File doesn't exist");
				if (!std::filesystem::is_regular_file(file_path))
					co_return Error("File isn't a regular file");
				if (!file_path.has_parent_path()) co_return Error("Suspicious file path without parent path");
			}
			catch (const std::filesystem::filesystem_error& e)
			{
				co_return Error::from(e);
			}

			const auto search_directory = file_path.parent_path();

			/* Parse */

			tinyobj::ObjReaderConfig reader_config;
			reader_config.triangulate = true;
			reader_config.mtl_search_path = search_directory.string();
			reader_config.vertex_color = false;

			tinyobj::ObjReader reader;
			if (!reader.ParseFromFile(file_path.string(), reader_config))
			{
				if (!reader.Error().empty())
					co_return Error("Parse model file failed", reader.Error());
				else
					co_return Error("Parse model file failed");
			}

			const auto texture_loader = [&search_directory](const std::string& path)
				-> std::variant<std::monostate, std::filesystem::path, std::vector<std::byte>> {
				auto replaced_path = path;
				std::ranges::replace(replaced_path, '\\', '/');  // Handle windows path

				auto full_path = search_directory / replaced_path;
				if (!std::filesystem::exists(full_path) || !std::filesystem::is_regular_file(full_path))
					return std::monostate{};

				return full_path;
			};

			co_return co_await load_from_parsed_data(thread_pool, *progress, reader, texture_loader);
		}
	}

	std::pair<coro::task<std::expected<Model, Error>>, std::shared_ptr<const Progress>> load_from_path(
		coro::thread_pool& thread_pool,
		const std::filesystem::path& file_path
	) noexcept
	{
		auto progress = std::make_shared<Progress>(Progress::from<ProgressState::Parsing>());
		auto task = load_from_path_impl(thread_pool, progress, file_path);
		return {std::move(task), progress};
	}
}

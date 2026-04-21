#include "model/gltf.hpp"
#include "asset.hpp"
#include "hierarchy.hpp"
#include "material.hpp"
#include "mesh.hpp"
#include "texture.hpp"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <filesystem>

namespace model::gltf
{
	namespace
	{
		coro::task<std::expected<Model, Error>> load_asset(
			coro::thread_pool& thread_pool,
			std::shared_ptr<Progress> progress,
			fastgltf::Asset asset,
			std::filesystem::path path
		) noexcept
		{
			/* Augment the asset */

			auto augmented_asset_result = impl::Asset::create(std::move(asset), path.parent_path());
			if (!augmented_asset_result)
				co_return augmented_asset_result.error().forward("Create asset failed");
			auto augmented_asset = std::move(*augmented_asset_result);

			/* Parse texture */

			progress->set<ProgressState::Material>();

			auto texture_result = impl::parse_textures(augmented_asset);
			if (!texture_result) co_return texture_result.error().forward("Parse textures failed");
			auto textures = std::move(*texture_result);

			/* Parse material */

			auto material_list_result = impl::parse_materials(augmented_asset, std::move(textures));
			if (!material_list_result)
				co_return material_list_result.error().forward("Parse materials failed");
			auto material_list = std::move(*material_list_result);

			/* Parse mesh */

			::util::Progress mesh_progress;
			progress->set<ProgressState::Mesh>(mesh_progress.get_ref());

			auto meshes_result = co_await impl::parse_meshes(thread_pool, mesh_progress, augmented_asset);
			if (!meshes_result) co_return meshes_result.error().forward("Parse meshes failed");
			auto meshes = std::move(*meshes_result);

			/* Create hierarchy */

			progress->set<ProgressState::Hierarchy>();

			auto hierarchy_result = impl::create_hierarchy(augmented_asset);
			if (!hierarchy_result) co_return hierarchy_result.error().forward("Create hierarchy failed");
			auto hierarchy = std::move(*hierarchy_result);

			co_return Model::assemble(std::move(material_list), std::move(meshes), std::move(hierarchy));
		}

		coro::task<std::expected<Model, Error>> load_from_file_impl(
			coro::thread_pool& thread_pool,
			std::shared_ptr<Progress> progress,
			std::filesystem::path path
		) noexcept
		{
			co_await thread_pool.schedule();

			progress->set<ProgressState::Parsing>();

			/* Validate path */

			if (!std::filesystem::exists(path))
				co_return Error("File doesn't exist", std::format("Path: {}", path.string()));

			if (!std::filesystem::is_regular_file(path))
				co_return Error("Path is not a regular file", std::format("Path: {}", path.string()));

			/* Load glTF */

			auto file_stream = fastgltf::GltfFileStream(path);
			if (!file_stream.isOpen())
				co_return Error("Open file failed", std::format("Path: {}", path.string()));

			auto result =
				fastgltf::Parser()
					.loadGltf(file_stream, path.parent_path(), fastgltf::Options::DecomposeNodeMatrices);
			if (!result)
			{
				co_return Error(
					"Parse glTF failed",
					std::format(
						"({}) {}",
						fastgltf::getErrorName(result.error()),
						fastgltf::getErrorMessage(result.error())
					)
				);
			}
			auto asset = std::move(result.get());

			co_return co_await load_asset(thread_pool, progress, std::move(asset), std::move(path));
		}

		coro::task<std::expected<Model, Error>> load_from_binary_impl(
			coro::thread_pool& thread_pool,
			std::shared_ptr<Progress> progress,
			std::vector<std::byte> data
		) noexcept
		{
			co_await thread_pool.schedule();

			progress->set<ProgressState::Parsing>();

			auto binary_stream_result = fastgltf::GltfDataBuffer::FromSpan(data);
			if (!binary_stream_result)
				co_return Error(
					"Create binary stream failed",
					std::format(
						"({}) {}",
						fastgltf::getErrorName(binary_stream_result.error()),
						fastgltf::getErrorMessage(binary_stream_result.error())
					)
				);
			auto binary_stream = std::move(binary_stream_result.get());

			auto result = fastgltf::Parser().loadGltf(
				binary_stream,
				std::filesystem::path(),
				fastgltf::Options::DecomposeNodeMatrices
			);
			if (!result)
			{
				co_return Error(
					"Parse glTF failed",
					std::format(
						"({}) {}",
						fastgltf::getErrorName(result.error()),
						fastgltf::getErrorMessage(result.error())
					)
				);
			}
			auto asset = std::move(result.get());

			co_return co_await load_asset(thread_pool, progress, std::move(asset), std::filesystem::path());
		}
	}

	[[nodiscard]]
	std::pair<coro::task<std::expected<Model, Error>>, std::shared_ptr<const Progress>> load_from_file(
		coro::thread_pool& thread_pool,
		const std::filesystem::path& path
	) noexcept
	{
		auto progress = std::make_shared<Progress>(Progress::from<ProgressState::Parsing>());
		return std::make_pair(load_from_file_impl(thread_pool, progress, path), progress);
	}

	[[nodiscard]]
	std::pair<coro::task<std::expected<Model, Error>>, std::shared_ptr<const Progress>> load_from_binary(
		coro::thread_pool& thread_pool,
		const std::vector<std::byte>& data
	) noexcept
	{
		auto progress = std::make_shared<Progress>(Progress::from<ProgressState::Parsing>());
		return std::make_pair(load_from_binary_impl(thread_pool, progress, data), progress);
	}
}

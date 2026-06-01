#pragma once

#include "common/util/error.hpp"
#include "common/util/tagged-type.hpp"
#include "model/hierarchy.hpp"
#include "model/model.hpp"
#include "render/model/blas.hpp"
#include "render/model/material.hpp"
#include "render/model/mesh.hpp"
#include "render/model/texture-list.hpp"
#include "vulkan/interface/context.hpp"

#include <coro/task.hpp>
#include <coro/thread_pool.hpp>
#include <expected>
#include <memory>
#include <utility>

namespace render
{
	///
	/// @brief All resources for a model, including hierarchy, meshes and materials.
	/// @warning Never move away the public members, as it will break integrety of the model
	///
	class Model
	{
	  public:

		///
		/// @brief State of loading model
		///
		///
		enum class ProgressState
		{
			Preparing,
			Material,
			Mesh,
			Blas,
		};

		///
		/// @brief Progress of loading model
		///
		///
		using Progress = util::SyncedEnumVariant<
			util::Tag<ProgressState::Preparing>,
			util::Tag<ProgressState::Material, std::shared_ptr<const MaterialList::Progress>>,
			util::Tag<ProgressState::Mesh>,
			util::Tag<ProgressState::Blas>
		>;

		///
		/// @brief Option of loading model
		/// @note Refer to docs of each entry for more details
		///
		struct Option
		{
			TextureList::LoadOption texture_load_option;
		};

		///
		/// @brief Load a model from a CPU-side `model::Model`
		///
		/// @warning The caller must eep the references alive and stable in address while loading
		///
		/// @param thread_pool Coroutine thread pool
		/// @param context Vulkan context
		/// @param material_layout Material layout of the model. Create one before loading a model
		/// @param model CPU-side model data
		/// @param option Option of loading model
		/// @return The coroutine task and a shared pointer to the progress
		///
		[[nodiscard]]
		static std::pair<coro::task<std::expected<Model, Error>>, std::shared_ptr<const Progress>> create(
			coro::thread_pool& thread_pool,
			const vulkan::Context& context,
			const MaterialLayout& material_layout,
			const model::Model& model,
			Option option = {}
		) noexcept;

		///
		/// @brief Hierarchy of the model
		///
		model::Hierarchy hierarchy;

		///
		/// @brief Mesh and primitive list of the model
		///
		MeshList mesh_list;

		///
		/// @brief Material list of the model
		///
		MaterialList material_list;

		///
		/// @brief BLAS list of the model
		/// @note For how the BLASes are arranged, see documentation of `BlasList`
		///
		BlasList blas_list;

	  private:

		explicit Model(
			model::Hierarchy hierarchy,
			MeshList mesh_list,
			MaterialList material_list,
			BlasList blas_list
		) :
			hierarchy(std::move(hierarchy)),
			mesh_list(std::move(mesh_list)),
			material_list(std::move(material_list)),
			blas_list(std::move(blas_list))
		{}

		[[nodiscard]]
		static coro::task<std::expected<Model, Error>> create_impl(
			coro::thread_pool& thread_pool,
			std::shared_ptr<Progress> progress,
			const vulkan::Context& context,
			const MaterialLayout& material_layout,
			const model::Model& model,
			Option option
		) noexcept;

		[[nodiscard]]
		static coro::task<std::expected<MeshList, Error>> create_mesh(
			coro::thread_pool& thread_pool,
			const vulkan::Context& context,
			const model::Model& model
		) noexcept;

		[[nodiscard]]
		static coro::task<std::expected<BlasList, Error>> create_blas(
			coro::thread_pool& thread_pool,
			const vulkan::Context& context,
			const MaterialList& material_list,
			const MeshList& mesh_list
		) noexcept;

	  public:

		Model(const Model&) = delete;
		Model(Model&&) = default;
		Model& operator=(const Model&) = delete;
		Model& operator=(Model&&) = default;
	};
}

#pragma once

#include "model/hierarchy.hpp"
#include "model/model.hpp"
#include "render/model/material.hpp"
#include "render/model/mesh.hpp"
#include "vulkan/interface/common-context.hpp"

#include <coro/task.hpp>

namespace render
{
	///
	/// @brief All resources for a model, including hierarchy, meshes and materials.
	/// @note Use `operator->` to access the hierarchy, meshes and materials.
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
			Mesh
		};

		///
		/// @brief Progress of loading model
		///
		///
		using Progress = util::SyncedEnumVariant<
			util::Tag<ProgressState::Preparing>,
			util::Tag<ProgressState::Material, std::shared_ptr<const MaterialList::Progress>>,
			util::Tag<ProgressState::Mesh>
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
			const vulkan::DeviceContext& context,
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

	  private:

		explicit Model(model::Hierarchy hierarchy, MeshList mesh_list, MaterialList material_list) :
			hierarchy(std::move(hierarchy)),
			mesh_list(std::move(mesh_list)),
			material_list(std::move(material_list))
		{}

		[[nodiscard]]
		static coro::task<std::expected<Model, Error>> create_impl(
			coro::thread_pool& thread_pool,
			std::shared_ptr<Progress> progress,
			const vulkan::DeviceContext& context,
			const MaterialLayout& material_layout,
			const model::Model& model,
			Option option
		) noexcept;

		[[nodiscard]]
		static coro::task<std::expected<MeshList, Error>> create_mesh(
			coro::thread_pool& thread_pool,
			const vulkan::DeviceContext& context,
			const model::Model& model
		) noexcept;

	  public:

		Model(const Model&) = delete;
		Model(Model&&) = default;
		Model& operator=(const Model&) = delete;
		Model& operator=(Model&&) = default;
	};
}

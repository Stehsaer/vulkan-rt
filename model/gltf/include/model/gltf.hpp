#pragma once

#include "common/util/async.hpp"
#include "common/util/error.hpp"
#include "common/util/tagged-type.hpp"
#include "model/model.hpp"

#include <coro/coro.hpp>

namespace model::gltf
{
	///
	/// @brief Progress state for glTF loading
	///
	///
	enum class ProgressState
	{
		Parsing,   // Parsing glTF file
		Material,  // Populating materials
		Mesh,      // Processing meshes
		Hierarchy  // Parsing hierarchy
	};

	using Progress = ::util::SyncedEnumVariant<
		::util::Tag<ProgressState::Parsing>,
		::util::Tag<ProgressState::Material>,
		::util::Tag<ProgressState::Mesh, ::util::Progress::Ref>,
		::util::Tag<ProgressState::Hierarchy>
	>;

	///
	/// @brief Load a glTF model from a file path
	///
	/// @param thread_pool Thread pool to use for asynchronous loading
	/// @param path File path to the glTF model
	/// @return A pair of a task that will yield the loaded model or an error, and a shared pointer to the
	/// progress state
	///
	[[nodiscard]]
	std::pair<coro::task<std::expected<Model, Error>>, std::shared_ptr<const Progress>> load_from_file(
		coro::thread_pool& thread_pool,
		const std::filesystem::path& path
	) noexcept;

	///
	/// @brief Load a glTF model from binary data
	///
	/// @param thread_pool Thread pool to use for asynchronous loading
	/// @param data Binary data containing the glTF model
	/// @return A pair of a task that will yield the loaded model or an error, and a shared pointer to the
	/// progress state
	///
	[[nodiscard]]
	std::pair<coro::task<std::expected<Model, Error>>, std::shared_ptr<const Progress>> load_from_binary(
		coro::thread_pool& thread_pool,
		const std::vector<std::byte>& data
	) noexcept;
}

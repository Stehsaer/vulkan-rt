#pragma once

#include <coro/coro.hpp>
#include <filesystem>
#include <mutex>

#include "common/util/async.hpp"
#include "common/util/error.hpp"
#include "common/util/tagged-type.hpp"
#include "model/model.hpp"

namespace model::obj
{
	///
	/// @brief Texture loader function
	///
	/// @details Takes texture path as input, returns one of:
	/// - `std::monostate` if the texture doesn't exist or fails to load
	/// - `std::filesystem::path` if the texture is loaded from a file path
	/// - `std::vector<std::byte>` if the texture is loaded from memory
	///
	using TextureLoader =
		std::variant<std::monostate, std::filesystem::path, std::vector<std::byte>>(const std::string& path);

	///
	/// @brief State of the loading progress
	///
	///
	enum class ProgressState
	{
		Parsing,             // Parsing the obj file
		ProcessingMesh,      // Processing the mesh data
		ProcessingTextures,  // Processing the textures
	};

	using Progress = ::util::SyncedEnumVariant<
		::util::Tag<ProgressState::Parsing>,
		::util::Tag<ProgressState::ProcessingMesh, ::util::Progress::Ref>,
		::util::Tag<ProgressState::ProcessingTextures>
	>;

	///
	/// @brief Load obj model from a file path
	/// @warning Due to the fact that obj file treats roughness and metalness textures separately,
	/// currently PBR material is not supported
	///
	/// @param thread_pool Thread pool to use for loading
	/// @param file_path File path
	/// @return Coroutine returning loaded model or error
	///
	[[nodiscard]]
	std::pair<coro::task<std::expected<Model, Error>>, std::shared_ptr<const Progress>> load_from_path(
		coro::thread_pool& thread_pool,
		const std::filesystem::path& file_path
	) noexcept;
}

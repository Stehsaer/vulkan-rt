#pragma once

#include "asset.hpp"
#include "common/util/async.hpp"
#include "common/util/error.hpp"
#include "model/mesh.hpp"

#include <coro/task.hpp>
#include <coro/thread_pool.hpp>
#include <fastgltf/types.hpp>

namespace model::gltf::impl
{
	[[nodiscard]]
	std::expected<Geometry, Error> parse_geometry(
		Asset& asset,
		const fastgltf::Primitive& primitive
	) noexcept;

	[[nodiscard]]
	coro::task<std::expected<Primitive, Error>> parse_primitive(
		coro::thread_pool& thread_pool,
		::util::Progress& progress,
		Asset& asset,
		const fastgltf::Primitive& primitive
	) noexcept;

	[[nodiscard]]
	coro::task<std::expected<Mesh, Error>> parse_mesh(
		coro::thread_pool& thread_pool,
		::util::Progress& progress,
		Asset& asset,
		const fastgltf::Mesh& mesh
	) noexcept;

	// Note: computationally heavy
	[[nodiscard]]
	coro::task<std::expected<std::vector<Mesh>, Error>> parse_meshes(
		coro::thread_pool& thread_pool,
		::util::Progress& progress,
		Asset& asset
	) noexcept;
}

#pragma once

#include "common/util/async.hpp"
#include "common/util/error.hpp"
#include "model/mesh.hpp"
#include "typedef.hpp"

#include <coro/task.hpp>
#include <coro/thread_pool.hpp>
#include <expected>
#include <glm/glm.hpp>
#include <libassert/assert.hpp>

namespace model::obj::impl
{
	// Separate indices according to material id
	[[nodiscard]]
	std::expected<PrimitiveMaterialMap, Error> separate_indices(const tinyobj::mesh_t& mesh) noexcept;

	// Get vector from list of float
	template <glm::length_t L>
		requires(L == 2 || L == 3)
	[[nodiscard]]
	glm::vec<L, float> get_vector(std::span<const float> attributes, uint32_t idx) noexcept
	{
		if constexpr (L == 2)
		{
			ASSUME(std::cmp_less(idx * 2 + 1, attributes.size()));
			return {attributes[idx * 2], attributes[idx * 2 + 1]};
		}
		else  // L == 3
		{
			ASSUME(std::cmp_less(idx * 3 + 2, attributes.size()));
			return {attributes[idx * 3], attributes[idx * 3 + 1], attributes[idx * 3 + 2]};
		}
	}

	// Get minimum vertex from attributes and a single index
	[[nodiscard]]
	std::expected<MinimumVertex, const char*> get_minimum_vertex(
		const tinyobj::attrib_t& attributes,
		const tinyobj::index_t& index,
		uint8_t index_in_face
	) noexcept;

	// Get or compute normal-only vertices
	[[nodiscard]]
	std::array<NormalOnlyVertex, 3> get_normal_only_vertices(
		const tinyobj::attrib_t& attributes,
		const std::array<MinimumVertex, 3>& min_vertices,
		const std::array<tinyobj::index_t, 3>& indices
	) noexcept;

	// Convert indices to geometry
	[[nodiscard]]
	std::expected<Geometry, Error> indices_to_geometry(
		const tinyobj::attrib_t& attributes,
		std::span<const TriangleIndices> indices
	) noexcept;

	// Convert mesh to `model::Mesh`
	[[nodiscard]]
	std::expected<Mesh, Error> convert_mesh(
		const tinyobj::attrib_t& attributes,
		const tinyobj::mesh_t& mesh
	) noexcept;

	// Async version of `convert_mesh`
	[[nodiscard]]
	coro::task<std::expected<Mesh, Error>> convert_mesh_async(
		coro::thread_pool& thread_pool,
		::util::Progress& progress,
		const tinyobj::attrib_t& attributes,
		const tinyobj::mesh_t& mesh
	) noexcept;
}

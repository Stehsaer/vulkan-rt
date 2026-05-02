#include "mesh.hpp"
#include "common/util/arithmetic-functor.hpp"
#include "common/util/unpack.hpp"

namespace model::obj::impl
{
	std::expected<PrimitiveMaterialMap, Error> separate_indices(const tinyobj::mesh_t& mesh) noexcept
	{
		if (!std::ranges::all_of(mesh.num_face_vertices, [](uint32_t count) { return count == 3; })
			|| mesh.indices.size() % 3 != 0)
			return Error("Invalid mesh with non-triangles");

		// Bind indices with material IDs
		auto face_material_list = std::vector(
			std::from_range,
			mesh.material_ids
				| std::views::enumerate
				| std::views::transform([&mesh](size_t face_index, int face_material_id) {
					  const auto indices = std::to_array(
						  {mesh.indices[face_index * 3],
						   mesh.indices[face_index * 3 + 1],
						   mesh.indices[face_index * 3 + 2]}
					  );
					  const auto key =
						  face_material_id < 0 ? std::nullopt : std::optional<uint32_t>(face_material_id);

					  return std::make_pair(key, indices);
				  } | ::util::tuple_args)
		);

		// Sort by material ids
		std::ranges::sort(
			face_material_list,
			std::ranges::less(),
			&std::pair<std::optional<uint32_t>, TriangleIndices>::first
		);

		// Chunk by material ids and convert to map
		auto result =
			face_material_list
			| std::views::chunk_by([](const auto& lhs, const auto& rhs) { return lhs.first == rhs.first; })
			| std::views::transform([](auto&& chunk) {
				  const auto material_id = chunk.front().first;
				  auto indices = chunk | std::views::values | std::ranges::to<std::vector>();
				  return std::make_pair(material_id, indices);
			  })
			| std::ranges::to<std::vector>();

		return result;
	}

	std::expected<MinimumVertex, const char*> get_minimum_vertex(
		const tinyobj::attrib_t& attributes,
		const tinyobj::index_t& index,
		uint8_t index_in_face
	) noexcept
	{
		static constexpr auto fallback_texcoord =
			std::to_array({glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 1.0f)});

		if (index.vertex_index < 0) [[unlikely]]
			return std::unexpected("Missing position attribute");

		if (std::cmp_greater_equal(index.vertex_index, attributes.vertices.size() / 3)) [[unlikely]]
			return std::unexpected("Position index out of bound");
		if (std::cmp_greater_equal(index.texcoord_index, attributes.texcoords.size() / 2)) [[unlikely]]
			return std::unexpected("Texcoord index out of bound");

		if (index.texcoord_index < 0)
		{
			return MinimumVertex{
				.position = get_vector<3>(attributes.vertices, index.vertex_index),
				.texcoord = fallback_texcoord[index_in_face]
			};
		}

		return MinimumVertex{
			.position = get_vector<3>(attributes.vertices, index.vertex_index),
			.texcoord = get_vector<2>(attributes.texcoords, index.texcoord_index)
		};
	}

	// Get or compute normal-only vertices
	std::array<NormalOnlyVertex, 3> get_normal_only_vertices(
		const tinyobj::attrib_t& attributes,
		const std::array<MinimumVertex, 3>& min_vertices,
		const std::array<tinyobj::index_t, 3>& indices
	) noexcept
	{
		if (std::ranges::all_of(indices, ::util::GreaterEqualToValue(0), &tinyobj::index_t::normal_index))
		{
			return std::to_array({
				NormalOnlyVertex::from(
					min_vertices[0],
					get_vector<3>(attributes.normals, indices[0].normal_index)
				),
				NormalOnlyVertex::from(
					min_vertices[1],
					get_vector<3>(attributes.normals, indices[1].normal_index)
				),
				NormalOnlyVertex::from(
					min_vertices[2],
					get_vector<3>(attributes.normals, indices[2].normal_index)
				),
			});
		}
		else
		{
			return MinimumVertex::construct_normal(min_vertices);
		}
	}

	// Convert indices to geometry
	std::expected<Geometry, Error> indices_to_geometry(
		const tinyobj::attrib_t& attributes,
		std::span<const TriangleIndices> indices
	) noexcept
	{
		std::vector<FullVertex> full_vertices;

		for (const auto tri : indices)
		{
			const auto min_v1_result = get_minimum_vertex(attributes, tri[0], 0);
			const auto min_v2_result = get_minimum_vertex(attributes, tri[1], 1);
			const auto min_v3_result = get_minimum_vertex(attributes, tri[2], 2);

			if (!min_v1_result) return Error("Process vertex failed", min_v1_result.error());
			if (!min_v2_result) return Error("Process vertex failed", min_v2_result.error());
			if (!min_v3_result) return Error("Process vertex failed", min_v3_result.error());

			const auto full = NormalOnlyVertex::construct_tangent(get_normal_only_vertices(
				attributes,
				std::to_array({*min_v1_result, *min_v2_result, *min_v3_result}),
				tri
			));

			full_vertices.append_range(full);
		}

		const auto full_indices =
			std::views::iota(0u, uint32_t(full_vertices.size())) | std::ranges::to<std::vector>();

		auto geometry_result = Geometry::create(full_vertices, full_indices);
		if (!geometry_result) return geometry_result.error().forward("Create geometry failed");
		return std::move(*geometry_result);
	}

	// Convert mesh to `model::Mesh`
	std::expected<Mesh, Error> convert_mesh(
		const tinyobj::attrib_t& attributes,
		const tinyobj::mesh_t& mesh
	) noexcept
	{
		/* Separate indices by material IDs */

		auto primitive_map_result = separate_indices(mesh);
		if (!primitive_map_result) return primitive_map_result.error().forward("Separate indices failed");
		auto primitive_map = std::move(*primitive_map_result);

		std::vector<Primitive> primitives;
		for (const auto& [material_id, geometry_data] : primitive_map)
		{
			auto geometry_result = indices_to_geometry(attributes, geometry_data);
			if (!geometry_result) return geometry_result.error().forward("Convert to geometry failed");
			auto geometry = std::move(*geometry_result);

			primitives.emplace_back(
				Primitive{.geometry = std::move(geometry), .material_index = material_id}
			);
		}

		return Mesh{.primitives = std::move(primitives)};
	}

	coro::task<std::expected<Mesh, Error>> convert_mesh_async(
		coro::thread_pool& thread_pool,
		::util::Progress& progress,
		const tinyobj::attrib_t& attributes,
		const tinyobj::mesh_t& mesh
	) noexcept
	{
		co_await thread_pool.schedule();
		auto result = convert_mesh(attributes, mesh);
		progress.increment();
		co_return result;
	};
}

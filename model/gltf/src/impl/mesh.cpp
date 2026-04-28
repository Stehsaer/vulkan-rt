#include "mesh.hpp"
#include "common/util/construct.hpp"
#include "model/mesh.hpp"

#include <coro/thread_pool.hpp>
#include <coro/when_all.hpp>
#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/tools.hpp>
#include <glm/fwd.hpp>
#include <ranges>

namespace model::gltf::impl
{
	std::expected<Geometry, Error> parse_geometry(Asset& asset, const fastgltf::Primitive& primitive) noexcept
	{
		/* Validate supported types */

		switch (primitive.type)
		{
		case fastgltf::PrimitiveType::Points:
		case fastgltf::PrimitiveType::Lines:
		case fastgltf::PrimitiveType::LineLoop:
		case fastgltf::PrimitiveType::LineStrip:
		default:
			return Error(
				"Primitive type not supported",
				std::format("Primitive type: {}", std::to_underlying(primitive.type))
			);

		case fastgltf::PrimitiveType::Triangles:
		case fastgltf::PrimitiveType::TriangleStrip:
		case fastgltf::PrimitiveType::TriangleFan:
			break;
		}

		/* POSITION */

		const auto position_attribute = primitive.findAttribute("POSITION");

		if (position_attribute == primitive.attributes.end())
			return Error("Missing POSITION attribute in primitive");

		const auto& position_accessor = asset.accessors[position_attribute->accessorIndex];
		std::vector<glm::vec3> positions(position_accessor.count);
		fastgltf::copyFromAccessor<glm::vec3>(
			asset,
			position_accessor,
			positions.data(),
			[&asset](const fastgltf::Asset&, size_t buffer_view_idx) {
				return asset.accessor_interface(asset, buffer_view_idx);
			}
		);

		/* Indices */

		std::vector<uint32_t> indices;
		if (!primitive.indicesAccessor.has_value())
		{
			indices = std::vector(std::from_range, std::views::iota(0u, position_accessor.count));
		}
		else
		{
			const auto& indices_accessor = asset.accessors[*primitive.indicesAccessor];
			indices.resize(indices_accessor.count);
			fastgltf::copyFromAccessor<uint32_t>(
				asset,
				indices_accessor,
				indices.data(),
				[&asset](const fastgltf::Asset&, size_t buffer_view_idx) {
					return asset.accessor_interface(asset, buffer_view_idx);
				}
			);
		}

		if (primitive.type == fastgltf::PrimitiveType::TriangleStrip)
		{
			auto result = util::expand_triangle_strip(indices);
			if (!result) return result.error().forward("Expand triangle strip failed");
			indices = std::move(*result);
		}

		if (primitive.type == fastgltf::PrimitiveType::TriangleFan)
		{
			auto result = util::expand_triangle_fan(indices);
			if (!result) return result.error().forward("Expand triangle fan failed");
			indices = std::move(*result);
		}

		if (indices.size() % 3 != 0)
		{
			return Error(
				"Invalid index count for triangle primitive",
				std::format("Index count: {}, expect a multiple of 3", indices.size())
			);
		}

		auto expanded_indices = std::vector(std::from_range, std::views::iota(0u, uint32_t(indices.size())));

		/* Expand POSITION */

		auto expanded_positions_result = util::expand_indices<glm::vec3>(indices, positions);
		if (!expanded_positions_result)
			return expanded_positions_result.error().forward("Expand position indices failed");
		positions = std::move(*expanded_positions_result);

		/* TEXCOORD, NORMAL and TANGENT */

		const auto texcoord_attribute = primitive.findAttribute("TEXCOORD_0");
		const bool has_texcoord = texcoord_attribute != primitive.attributes.end();
		std::vector<glm::vec2> texcoords;
		if (has_texcoord)
		{
			const auto& texcoord_accessor = asset.accessors[texcoord_attribute->accessorIndex];
			texcoords.resize(texcoord_accessor.count);
			fastgltf::copyFromAccessor<glm::vec2>(
				asset,
				texcoord_accessor,
				texcoords.data(),
				[&asset](const fastgltf::Asset&, size_t buffer_view_idx) {
					return asset.accessor_interface(asset, buffer_view_idx);
				}
			);

			auto expanded_texcoords_result = util::expand_indices<glm::vec2>(indices, texcoords);
			if (!expanded_texcoords_result)
				return expanded_texcoords_result.error().forward("Expand texcoord indices failed");
			texcoords = std::move(*expanded_texcoords_result);
		}
		else
		{
			static constexpr auto fallback =
				std::to_array({glm::vec2(0.0f, 0.0f), glm::vec2(1.0f, 0.0f), glm::vec2(0.0f, 1.0f)});

			texcoords.resize(indices.size());
			std::ranges::copy(
				std::views::repeat(fallback) | std::views::join | std::views::take(indices.size()),
				texcoords.begin()
			);
		}

		const auto normal_attribute = primitive.findAttribute("NORMAL");
		const bool has_normal = normal_attribute != primitive.attributes.end();
		std::vector<glm::vec3> normals;
		if (has_normal)
		{
			const auto& normal_accessor = asset.accessors[normal_attribute->accessorIndex];
			normals.resize(normal_accessor.count);
			fastgltf::copyFromAccessor<glm::vec3>(
				asset,
				normal_accessor,
				normals.data(),
				[&asset](const fastgltf::Asset&, size_t buffer_view_idx) {
					return asset.accessor_interface(asset, buffer_view_idx);
				}
			);

			auto expanded_normals_result = util::expand_indices<glm::vec3>(indices, normals);
			if (!expanded_normals_result)
				return expanded_normals_result.error().forward("Expand normal indices failed");
			normals = std::move(*expanded_normals_result);
		}

		const auto tangent_attribute = primitive.findAttribute("TANGENT");
		const bool has_tangent = tangent_attribute != primitive.attributes.end();
		std::vector<glm::vec4> tangents;
		if (has_tangent)
		{
			const auto& tangent_accessor = asset.accessors[tangent_attribute->accessorIndex];
			tangents.resize(tangent_accessor.count);
			fastgltf::copyFromAccessor<glm::vec4>(
				asset,
				tangent_accessor,
				tangents.data(),
				[&asset](const fastgltf::Asset&, size_t buffer_view_idx) {
					return asset.accessor_interface(asset, buffer_view_idx);
				}
			);

			auto expanded_tangents_result = util::expand_indices<glm::vec4>(indices, tangents);
			if (!expanded_tangents_result)
				return expanded_tangents_result.error().forward("Expand tangent indices failed");
			tangents = std::move(*expanded_tangents_result);
		}

		/* Calculate missing values */

		// Full vertex
		if (has_tangent && has_normal && has_texcoord)
		{
			auto vertices =
				std::views::zip_transform(CTOR_LAMBDA(FullVertex), positions, texcoords, normals, tangents)
				| std::ranges::to<std::vector>();

			return Geometry::create(std::move(vertices), std::move(expanded_indices));
		}

		// Normal only vertex
		if (has_normal)
		{
			auto vertices =
				std::views::zip_transform(CTOR_LAMBDA(NormalOnlyVertex), positions, texcoords, normals)
				| std::views::chunk(3)
				| std::views::transform([](const auto& triangle) {
					  return NormalOnlyVertex::construct_tangent(
						  std::to_array({triangle[0], triangle[1], triangle[2]})
					  );
				  })
				| std::views::join
				| std::ranges::to<std::vector>();

			return Geometry::create(std::move(vertices), std::move(expanded_indices));
		}

		// Minimum vertex

		auto vertices =
			std::views::zip_transform(CTOR_LAMBDA(MinimumVertex), positions, texcoords)
			| std::views::adjacent<3>
			| std::views::transform([](const auto& triangle) {
				  return NormalOnlyVertex::construct_tangent(
					  MinimumVertex::construct_normal(
						  std::to_array({std::get<0>(triangle), std::get<1>(triangle), std::get<2>(triangle)})
					  )
				  );
			  })
			| std::views::join
			| std::ranges::to<std::vector>();

		return Geometry::create(std::move(vertices), std::move(expanded_indices));
	}

	coro::task<std::expected<Primitive, Error>> parse_primitive(
		coro::thread_pool& thread_pool,
		::util::Progress& progress,
		Asset& asset,
		const fastgltf::Primitive& primitive
	) noexcept
	{
		co_await thread_pool.schedule();

		auto geometry_result = parse_geometry(asset, primitive);
		progress.increment();
		if (!geometry_result) co_return geometry_result.error().forward("Parse geometry failed");

		co_return Primitive{
			.geometry = std::move(*geometry_result),
			.material_index =
				primitive.materialIndex.transform([](size_t idx) { return static_cast<uint32_t>(idx); }),
		};
	}

	coro::task<std::expected<Mesh, Error>> parse_mesh(
		coro::thread_pool& thread_pool,
		::util::Progress& progress,
		Asset& asset,
		const fastgltf::Mesh& mesh
	) noexcept
	{
		co_await thread_pool.schedule();

		auto primitive_tasks =
			mesh.primitives
			| std::views::transform([&thread_pool, &progress, &asset](const auto& primitive) {
				  return parse_primitive(thread_pool, progress, asset, primitive);
			  })
			| std::ranges::to<std::vector>();

		auto primitive_result = co_await coro::when_all(std::move(primitive_tasks))
			| std::views::transform([](auto&& result) { return result.return_value(); })
			| Error::collect();

		if (!primitive_result) co_return primitive_result.error().forward("Parse primitives failed");

		co_return Mesh{.primitives = std::move(*primitive_result)};
	}

	coro::task<std::expected<std::vector<Mesh>, Error>> parse_meshes(
		coro::thread_pool& thread_pool,
		::util::Progress& progress,
		Asset& asset
	) noexcept
	{
		const auto total_primitives = std::ranges::fold_left(
			asset.meshes | std::views::transform([](const auto& mesh) { return mesh.primitives.size(); }),
			0zu,
			std::plus{}
		);
		progress.set_total(total_primitives);

		auto mesh_tasks =
			asset.meshes
			| std::views::transform([&](const auto& mesh) {
				  return parse_mesh(thread_pool, progress, asset, mesh);
			  })
			| std::ranges::to<std::vector>();

		auto mesh_result = co_await coro::when_all(std::move(mesh_tasks))
			| std::views::transform([](auto&& result) { return result.return_value(); })
			| Error::collect();

		if (!mesh_result) co_return mesh_result.error().forward("Parse meshes failed");

		co_return *mesh_result;
	}
}

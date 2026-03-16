#include "model.hpp"
#include "common/util/span.hpp"
#include "vulkan/util/static-resource-creator.hpp"

#include <format>
#include <tiny_obj_loader.h>
#include <utility>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace
{
	struct PartialVertex
	{
		glm::vec3 position;
		glm::vec2 texcoord;
		std::optional<glm::vec3> normal;

		static void fill_normal(PartialVertex& v1, PartialVertex& v2, PartialVertex& v3) noexcept
		{
			if (v1.normal && v2.normal && v3.normal) return;

			const auto edge1 = v2.position - v1.position;
			const auto edge2 = v3.position - v1.position;
			const auto normal = glm::normalize(glm::cross(edge1, edge2));

			v1.normal = normal;
			v2.normal = normal;
			v3.normal = normal;
		}

		[[nodiscard]]
		Vertex as_vertex() const
		{
			return Vertex{.position = position, .uv = texcoord, .normal = normal.value_or(glm::vec3(0.0))};
		}
	};

	struct AttributeData
	{
		std::vector<glm::vec3> positions;
		std::vector<glm::vec2> texcoords;
		std::vector<glm::vec3> normals;

		[[nodiscard]]
		static AttributeData create(const tinyobj::attrib_t& attrib) noexcept
		{
			std::vector<glm::vec3> positions(attrib.vertices.size() / 3);
			std::vector<glm::vec2> texcoords(attrib.texcoords.size() / 2);
			std::vector<glm::vec3> normals(attrib.normals.size() / 3);

			for (const auto [idx, position] : std::views::enumerate(positions))
			{
				position.x = attrib.vertices[idx * 3];
				position.y = attrib.vertices[idx * 3 + 1];
				position.z = attrib.vertices[idx * 3 + 2];
			}

			for (const auto [idx, texcoord] : std::views::enumerate(texcoords))
			{
				texcoord.x = attrib.texcoords[idx * 2];
				texcoord.y = attrib.texcoords[idx * 2 + 1];
			}

			for (const auto [idx, normal] : std::views::enumerate(normals))
			{
				normal.x = attrib.normals[idx * 3];
				normal.y = attrib.normals[idx * 3 + 1];
				normal.z = attrib.normals[idx * 3 + 2];
			}

			return {
				.positions = std::move(positions),
				.texcoords = std::move(texcoords),
				.normals = std::move(normals)
			};
		}

		[[nodiscard]]
		std::optional<PartialVertex> get(int position, int texcoord, int normal) const noexcept
		{
			if (std::cmp_greater_equal(position, positions.size()) || position < 0) return std::nullopt;
			if (std::cmp_greater_equal(texcoord, texcoords.size())) return std::nullopt;
			if (std::cmp_greater_equal(normal, normals.size())) return std::nullopt;

			return PartialVertex{
				.position = positions[position],
				.texcoord = texcoord < 0 ? glm::vec2(0.0) : texcoords[texcoord],
				.normal = normal < 0 ? std::optional<glm::vec3>(std::nullopt) : normals[normal]
			};
		}
	};

	struct TinybobjResult
	{
		AttributeData attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;

		[[nodiscard]]
		static std::expected<TinybobjResult, Error> load(const std::string& path) noexcept
		{
			tinyobj::attrib_t attrib;
			std::vector<tinyobj::shape_t> shapes;
			std::vector<tinyobj::material_t> materials;

			std::string warn;
			std::string err;

			if (!tinyobj::LoadObj(
					&attrib,
					&shapes,
					&materials,
					&warn,
					&err,
					path.c_str(),
					nullptr,
					true /* Triangulate enabled */
				))
			{
				if (!err.empty()) return Error("Error parsing model file", err);
				return Error("Error parsing model file", "Unknown error");
			}

			auto attrib_data = AttributeData::create(attrib);

			return TinybobjResult{
				.attrib = std::move(attrib_data),
				.shapes = std::move(shapes),
				.materials = std::move(materials)
			};
		}

		[[nodiscard]]
		std::expected<std::vector<Vertex>, Error> get_vertices() const noexcept
		{
			std::vector<Vertex> vertices;

			auto renderable_shapes =
				shapes | std::views::filter([](const tinyobj::shape_t& shape) {
					return !shape.mesh.num_face_vertices.empty();
				});

			for (const auto& shape : renderable_shapes)
			{
				const auto& mesh = shape.mesh;

				// Check all triangles
				if (!std::ranges::all_of(mesh.num_face_vertices, [](uint32_t vertices) {
						return vertices == 3;
					}))
				{
					return Error(
						"Triangulation unexpectedly failed",
						std::format("Remaining non-triangle found in shape {:?}", shape.name)
					);
				}

				// Extract all triangles
				for (const auto face_idx : std::views::iota(0zu, mesh.num_face_vertices.size()))
				{
					const auto& idx0 = mesh.indices[3 * face_idx + 0];
					const auto& idx1 = mesh.indices[3 * face_idx + 1];
					const auto& idx2 = mesh.indices[3 * face_idx + 2];

					auto vert0_result = attrib.get(idx0.vertex_index, idx0.texcoord_index, idx0.normal_index);
					auto vert1_result = attrib.get(idx1.vertex_index, idx1.texcoord_index, idx1.normal_index);
					auto vert2_result = attrib.get(idx2.vertex_index, idx2.texcoord_index, idx2.normal_index);

					if (!vert0_result || !vert1_result || !vert2_result)
					{
						return Error(
							"Invalid vertex index",
							std::format(
								"Face {} in shape {} has invalid vertex indices",
								face_idx,
								shape.name
							)
						);
					}

					auto vert0 = *vert0_result;
					auto vert1 = *vert1_result;
					auto vert2 = *vert2_result;

					PartialVertex::fill_normal(vert0, vert1, vert2);

					vertices.emplace_back(vert0.as_vertex());
					vertices.emplace_back(vert1.as_vertex());
					vertices.emplace_back(vert2.as_vertex());
				}
			}

			return vertices;
		}
	};
}

std::expected<Model, Error> Model::load_from_file(const std::string_view& path) noexcept
{
	auto tinyobj_result = TinybobjResult::load(std::string(path));
	if (!tinyobj_result) return tinyobj_result.error().forward("Load model file failed");
	auto vertices_result = tinyobj_result->get_vertices();
	if (!vertices_result)
		return vertices_result.error().forward("Extract vertices from tinyobj result failed");
	auto vertices = std::move(*vertices_result);

	return Model{
		.indices =
			std::views::iota(0u, static_cast<unsigned int>(vertices.size())) | std::ranges::to<std::vector>(),
		.vertices = std::move(vertices),
	};
}

ModelBuffer ModelBuffer::create(const vulkan::DeviceContext& context, const Model& model)
{
	auto resource_creator = vulkan::StaticResourceCreator(context);

	auto vertex_buffer =
		resource_creator.create_buffer(util::as_bytes(model.vertices), vk::BufferUsageFlagBits::eVertexBuffer)
		| Error::unwrap("Create vertex buffer failed");

	auto index_buffer =
		resource_creator.create_buffer(util::as_bytes(model.indices), vk::BufferUsageFlagBits::eIndexBuffer)
		| Error::unwrap("Create index buffer failed");

	resource_creator.execute_uploads() | Error::unwrap("Execute model buffer uploads failed");

	return ModelBuffer{
		.vertex_buffer = std::move(vertex_buffer),
		.index_buffer = std::move(index_buffer),
		.vertex_count = static_cast<uint32_t>(model.indices.size()),
	};
}

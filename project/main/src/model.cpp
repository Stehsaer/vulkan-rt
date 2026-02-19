#include "model.hpp"
#include "common/util/span-util.hpp"
#include "vulkan/alloc.hpp"
#include "vulkan/util/uploader.hpp"

#include <array>
#include <filesystem>
#include <format>
#include <rapidobj/rapidobj.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

static bool index_has_normal(const rapidobj::Index& index) noexcept
{
	return index.normal_index >= 0;
};

static glm::vec3 get_position(const rapidobj::Attributes& attributes, const rapidobj::Index& index) noexcept
{
	return {
		attributes.positions[3 * index.position_index + 0],
		attributes.positions[3 * index.position_index + 1],
		attributes.positions[3 * index.position_index + 2]
	};
}

static glm::vec2 get_uv(const rapidobj::Attributes& attributes, const rapidobj::Index& index) noexcept
{
	if (index.texcoord_index < 0) return {0.0f, 0.0f};

	return {
		attributes.texcoords[2 * index.texcoord_index + 0],
		attributes.texcoords[2 * index.texcoord_index + 1]
	};
}

static glm::vec3 get_normal(const rapidobj::Attributes& attributes, const rapidobj::Index& index) noexcept
{
	return {
		attributes.normals[3 * index.normal_index + 0],
		attributes.normals[3 * index.normal_index + 1],
		attributes.normals[3 * index.normal_index + 2]
	};
}

static std::expected<std::vector<Vertex>, Error> extract_vertices(
	const rapidobj::Result& parse_result
) noexcept
{
	std::vector<Vertex> vertices;

	for (const auto& shape : parse_result.shapes)
	{
		const auto& mesh = shape.mesh;

		if (!std::ranges::all_of(mesh.num_face_vertices, [](int num_vertices) { return num_vertices == 3; }))
			return Error("Model contains non-triangular faces");
		if (mesh.indices.size() % 3 != 0) return Error("Model contains incomplete faces");

		for (const auto triangle : mesh.indices | std::views::chunk(3))
		{
			std::array<Vertex, 3> vert = {
				Vertex{
					   .position = get_position(parse_result.attributes, triangle[0]),
					   .uv = get_uv(parse_result.attributes, triangle[0]),
					   .normal = glm::vec3(0.0f, 0.0f, 0.0f)
				},
				Vertex{
					   .position = get_position(parse_result.attributes, triangle[1]),
					   .uv = get_uv(parse_result.attributes, triangle[1]),
					   .normal = glm::vec3(0.0f, 0.0f, 0.0f)
				},
				Vertex{
					   .position = get_position(parse_result.attributes, triangle[2]),
					   .uv = get_uv(parse_result.attributes, triangle[2]),
					   .normal = glm::vec3(0.0f, 0.0f, 0.0f)
				},
			};

			if (std::ranges::all_of(triangle, index_has_normal))
			{
				vert[0].normal = get_normal(parse_result.attributes, triangle[0]);
				vert[1].normal = get_normal(parse_result.attributes, triangle[1]);
				vert[2].normal = get_normal(parse_result.attributes, triangle[2]);
			}
			else
			{
				const auto edge1 = vert[1].position - vert[0].position;
				const auto edge2 = vert[2].position - vert[0].position;
				const auto normal = glm::normalize(glm::cross(edge1, edge2));

				vert[0].normal = normal;
				vert[1].normal = normal;
				vert[2].normal = normal;
			}

			vertices.append_range(vert);
		}
	}

	return vertices;
}

std::expected<Model, Error> Model::load_from_file(const std::string_view& path) noexcept
{
	auto parse_result =
		rapidobj::ParseFile(std::filesystem::path(path), rapidobj::MaterialLibrary::Default());
	if (parse_result.error)
		return Error("Error parsing model file", std::format("{}", parse_result.error.code.message()));
	if (!rapidobj::Triangulate(parse_result)) return Error("Triangulation failed");

	auto vertices_result = extract_vertices(parse_result);
	if (!vertices_result) return vertices_result.error();
	auto vertices = std::move(*vertices_result);

	return Model{
		.indices =
			std::views::iota(0u, static_cast<unsigned int>(vertices.size())) | std::ranges::to<std::vector>(),
		.vertices = std::move(vertices),
	};
}

ModelBuffer ModelBuffer::create(const vulkan::DeviceContext& context, const Model& model)
{
	auto vertex_buffer =
		context.allocator.create_buffer(
			vk::BufferCreateInfo{
				.size = util::as_bytes(model.vertices).size(),
				.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
				.sharingMode = vk::SharingMode::eExclusive
			},
			vulkan::alloc::MemoryUsage::GpuOnly
		)
		| Error::unwrap("Create vertex buffer failed");

	auto index_buffer =
		context.allocator.create_buffer(
			vk::BufferCreateInfo{
				.size = util::as_bytes(model.indices).size(),
				.usage = vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
				.sharingMode = vk::SharingMode::eExclusive
			},
			vulkan::alloc::MemoryUsage::GpuOnly
		)
		| Error::unwrap("Create index buffer failed");

	auto uploader = vulkan::Uploader(
		context.device,
		*context.render_queue.queue,
		context.render_queue.family_index,
		context.allocator
	);

	uploader.upload_buffer(
		vulkan::Uploader::BufferUploadParam{
			.dst_buffer = vertex_buffer,
			.data = util::as_bytes(model.vertices)
		}
	) | Error::unwrap("Upload vertex buffer failed");

	uploader.upload_buffer(
		vulkan::Uploader::BufferUploadParam{.dst_buffer = index_buffer, .data = util::as_bytes(model.indices)}
	) | Error::unwrap("Upload index buffer failed");

	uploader.execute() | Error::unwrap("Execute buffer upload failed");

	return ModelBuffer{
		.vertex_buffer = std::move(vertex_buffer),
		.index_buffer = std::move(index_buffer),
		.vertex_count = static_cast<uint32_t>(model.indices.size()),
	};
}

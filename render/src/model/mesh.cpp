#include "render/model/mesh.hpp"
#include "common/util/span.hpp"
#include "vulkan/util/static-resource-creator.hpp"
#include <ranges>

namespace render
{
	std::expected<MeshList, Error> MeshList::create(
		const vulkan::DeviceContext& context,
		std::span<const model::Mesh> mesh
	) noexcept
	{
		/* Step 1: Collect data */

		std::vector<model::FullVertex> vertices;
		std::vector<uint32_t> indices;
		std::vector<PrimitiveAttribute> primitive_attrs;
		std::vector<PrimitiveIndexRange> mesh_primitive_index_ranges;

		const auto all_primitives = mesh | std::views::transform(&model::Mesh::primitives) | std::views::join;
		const auto vertex_count = std::ranges::fold_left(
			all_primitives | std::views::transform([](const model::Primitive& primitive) {
				return primitive.geometry.vertices.size();
			}),
			0zu,
			std::plus()
		);
		const auto index_count = std::ranges::fold_left(
			all_primitives | std::views::transform([](const model::Primitive& primitive) {
				return primitive.geometry.indices.size();
			}),
			0zu,
			std::plus()
		);
		const auto primitive_attr_count = std::ranges::distance(all_primitives);

		vertices.reserve(vertex_count);
		indices.reserve(index_count);
		primitive_attrs.reserve(primitive_attr_count);
		mesh_primitive_index_ranges.reserve(mesh.size());

		const auto push_primitive = [&vertices, &indices](const model::Primitive& primitive) {
			const auto index_offset = static_cast<uint32_t>(indices.size());
			const auto vertex_offset = static_cast<uint32_t>(vertices.size());

			vertices.append_range(primitive.geometry.vertices);
			indices.append_range(primitive.geometry.indices);

			return PrimitiveAttribute{
				.index_offset = index_offset,
				.vertex_offset = vertex_offset,
				.vertex_count = static_cast<uint32_t>(primitive.geometry.vertices.size()),
				.material_index = primitive.material_index.value_or(DEFAULT_MATERIAL),
				.aabb_min = primitive.geometry.aabb_min,
				.aabb_max = primitive.geometry.aabb_max
			};
		};

		for (const auto& mesh : mesh)
		{
			const auto primitive_offset = static_cast<uint32_t>(primitive_attrs.size());
			primitive_attrs.append_range(mesh.primitives | std::views::transform(push_primitive));

			mesh_primitive_index_ranges.push_back(
				PrimitiveIndexRange{
					.offset = primitive_offset,
					.count = static_cast<uint32_t>(mesh.primitives.size())
				}
			);
		}

		/* Step 2: Create buffers & upload */

		vulkan::StaticResourceCreator resource_creator(context);

		auto vertex_buffer_result =
			resource_creator.create_array_buffer(vertices, vk::BufferUsageFlagBits::eVertexBuffer);
		auto index_buffer_result =
			resource_creator.create_array_buffer(indices, vk::BufferUsageFlagBits::eIndexBuffer);
		auto primitive_attr_buffer_result =
			resource_creator.create_array_buffer(primitive_attrs, vk::BufferUsageFlagBits::eStorageBuffer);

		if (!vertex_buffer_result) return vertex_buffer_result.error().forward("Create vertex buffer failed");
		if (!index_buffer_result) return index_buffer_result.error().forward("Create index buffer failed");
		if (!primitive_attr_buffer_result)
			return primitive_attr_buffer_result.error().forward("Create primitive buffer failed");

		auto vertex_buffer = std::move(*vertex_buffer_result);
		auto index_buffer = std::move(*index_buffer_result);
		auto primitive_attr_buffer = std::move(*primitive_attr_buffer_result);

		if (const auto upload_result = resource_creator.execute_uploads(); !upload_result)
			return upload_result.error().forward("Execute upload tasks failed");

		return MeshList(
			std::move(vertex_buffer),
			std::move(index_buffer),
			std::move(primitive_attr_buffer),
			std::move(mesh_primitive_index_ranges),
			std::move(primitive_attrs)
		);
	}
}

#include "render/model/mesh.hpp"
#include "common/util/error.hpp"
#include "model/mesh.hpp"
#include "vulkan/alloc/buffer.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/util/static-resource-creator.hpp"

#include <algorithm>
#include <cstdint>
#include <expected>
#include <functional>
#include <iterator>
#include <ranges>
#include <span>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	namespace
	{
		struct MeshCollectResult
		{
			std::vector<model::FullVertex> vertices;
			std::vector<uint32_t> indices;
			std::vector<PrimitiveAttribute> primitive_attrs;
			std::vector<PrimitiveIndexRange> mesh_primitive_index_ranges;
		};

		MeshCollectResult collect_mesh_data(std::span<const model::Mesh> mesh) noexcept
		{
			std::vector<model::FullVertex> vertices;
			std::vector<uint32_t> indices;
			std::vector<PrimitiveAttribute> primitive_attrs;
			std::vector<PrimitiveIndexRange> mesh_primitive_index_ranges;

			const auto all_primitives =
				mesh | std::views::transform(&model::Mesh::primitives) | std::views::join;
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
					.index_count = static_cast<uint32_t>(primitive.geometry.indices.size()),
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

			return {
				.vertices = std::move(vertices),
				.indices = std::move(indices),
				.primitive_attrs = std::move(primitive_attrs),
				.mesh_primitive_index_ranges = std::move(mesh_primitive_index_ranges),
			};
		}

		struct BufferResult
		{
			vulkan::ArrayBuffer<model::FullVertex> vertex_buffer;
			vulkan::ArrayBuffer<uint32_t> index_buffer;
			vulkan::ArrayBuffer<PrimitiveAttribute> primitive_attr_buffer;
		};

		std::expected<BufferResult, Error> create_buffers(
			const vulkan::Context& context,
			const MeshCollectResult& collect_result
		) noexcept
		{
			auto resource_creator_result = vulkan::StaticResourceCreator::create(context);
			if (!resource_creator_result)
				return resource_creator_result.error().forward("Create resource creator failed");
			auto resource_creator = std::move(*resource_creator_result);

			vk::BufferUsageFlags geometry_buffer_extra_flgs = vk::BufferUsageFlagBits::eShaderDeviceAddress;
			if (context.feature.raytracing)
				geometry_buffer_extra_flgs |=
					vk::BufferUsageFlagBits::eAccelerationStructureBuildInputReadOnlyKHR;

			auto vertex_buffer_result = resource_creator.create_array_buffer(
				context,
				collect_result.vertices,
				vk::BufferUsageFlagBits::eVertexBuffer | geometry_buffer_extra_flgs
			);
			auto index_buffer_result = resource_creator.create_array_buffer(
				context,
				collect_result.indices,
				vk::BufferUsageFlagBits::eIndexBuffer | geometry_buffer_extra_flgs
			);
			auto primitive_attr_buffer_result = resource_creator.create_array_buffer(
				context,
				collect_result.primitive_attrs,
				vk::BufferUsageFlagBits::eStorageBuffer
			);

			if (!vertex_buffer_result)
				return vertex_buffer_result.error().forward("Create vertex buffer failed");
			if (!index_buffer_result)
				return index_buffer_result.error().forward("Create index buffer failed");
			if (!primitive_attr_buffer_result)
				return primitive_attr_buffer_result.error().forward("Create primitive buffer failed");

			auto vertex_buffer = std::move(*vertex_buffer_result);
			auto index_buffer = std::move(*index_buffer_result);
			auto primitive_attr_buffer = std::move(*primitive_attr_buffer_result);

			if (const auto upload_result = resource_creator.execute_uploads(context); !upload_result)
				return upload_result.error().forward("Execute upload tasks failed");

			return BufferResult{
				.vertex_buffer = std::move(vertex_buffer),
				.index_buffer = std::move(index_buffer),
				.primitive_attr_buffer = std::move(primitive_attr_buffer),
			};
		}

	}

	std::expected<MeshList, Error> MeshList::create(
		const vulkan::Context& context,
		std::span<const model::Mesh> mesh
	) noexcept
	{
		auto mesh_collect_result = collect_mesh_data(mesh);

		auto buffers_result = create_buffers(context, mesh_collect_result);
		if (!buffers_result) return buffers_result.error();
		auto buffers = std::move(*buffers_result);

		return MeshList(
			std::move(buffers.vertex_buffer),
			std::move(buffers.index_buffer),
			std::move(buffers.primitive_attr_buffer),
			std::move(mesh_collect_result.mesh_primitive_index_ranges),
			std::move(mesh_collect_result.primitive_attrs)
		);
	}
}

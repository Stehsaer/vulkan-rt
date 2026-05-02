#include "model/mesh.hpp"

#include <algorithm>
#include <glm/gtx/vector_angle.hpp>
#include <glm/vector_relational.hpp>
#include <meshoptimizer.h>
#include <ranges>

namespace model
{
	std::array<NormalOnlyVertex, 3> MinimumVertex::construct_normal(
		const std::array<MinimumVertex, 3>& triangle
	) noexcept
	{
		const auto edge1 = triangle[1].position - triangle[0].position;
		const auto edge2 = triangle[2].position - triangle[0].position;
		const auto normal = glm::normalize(glm::cross(edge1, edge2));

		if (glm::any(glm::isinf(normal)) || glm::any(glm::isnan(normal))) [[unlikely]]
			return {
				NormalOnlyVertex{
								 .position = triangle[0].position,
								 .texcoord = triangle[0].texcoord,
								 .normal = glm::vec3(0.0f),
								 },
				NormalOnlyVertex{
								 .position = triangle[1].position,
								 .texcoord = triangle[1].texcoord,
								 .normal = glm::vec3(0.0f),
								 },
				NormalOnlyVertex{
								 .position = triangle[2].position,
								 .texcoord = triangle[2].texcoord,
								 .normal = glm::vec3(0.0f),
								 }
			};

		return {
			NormalOnlyVertex{
							 .position = triangle[0].position,
							 .texcoord = triangle[0].texcoord,
							 .normal = normal
			},
			NormalOnlyVertex{
							 .position = triangle[1].position,
							 .texcoord = triangle[1].texcoord,
							 .normal = normal
			},
			NormalOnlyVertex{
							 .position = triangle[2].position,
							 .texcoord = triangle[2].texcoord,
							 .normal = normal
			}
		};
	}

	std::array<FullVertex, 3> NormalOnlyVertex::construct_tangent(
		const std::array<NormalOnlyVertex, 3>& triangle
	) noexcept
	{
		const glm::mat2x3 pos_delta(
			triangle[1].position - triangle[0].position,
			triangle[2].position - triangle[0].position
		);

		const glm::mat2 texcoord_delta(
			triangle[1].texcoord - triangle[0].texcoord,
			triangle[2].texcoord - triangle[0].texcoord
		);
		const glm::mat2 texcoord_delta_inv = glm::inverse(texcoord_delta);
		const glm::mat2x3 tangent_mat = pos_delta * texcoord_delta_inv;

		const glm::vec3 tangent = glm::normalize(tangent_mat[0]);
		const glm::vec3 bitangent = glm::normalize(tangent_mat[1]);
		const glm::vec3 normal = triangle[0].normal;

		const glm::vec3 right_handed_bitangent = glm::cross(normal, tangent);
		const float handedness = glm::dot(right_handed_bitangent, bitangent) < 0.0f ? -1.0f : 1.0f;

		const glm::vec4 encoded_tangent = glm::vec4(tangent, handedness);

		if (glm::any(glm::isinf(tangent)) || glm::any(glm::isnan(tangent))) [[unlikely]]
			return {
				FullVertex{
						   .position = triangle[0].position,
						   .texcoord = triangle[0].texcoord,
						   .normal = triangle[0].normal,
						   .tangent = glm::vec4(0.0f),
						   },
				FullVertex{
						   .position = triangle[1].position,
						   .texcoord = triangle[1].texcoord,
						   .normal = triangle[1].normal,
						   .tangent = glm::vec4(0.0f),
						   },
				FullVertex{
						   .position = triangle[2].position,
						   .texcoord = triangle[2].texcoord,
						   .normal = triangle[2].normal,
						   .tangent = glm::vec4(0.0f),
						   }
			};

		return {
			FullVertex{
					   .position = triangle[0].position,
					   .texcoord = triangle[0].texcoord,
					   .normal = triangle[0].normal,
					   .tangent = encoded_tangent,
					   },
			FullVertex{
					   .position = triangle[1].position,
					   .texcoord = triangle[1].texcoord,
					   .normal = triangle[1].normal,
					   .tangent = encoded_tangent,
					   },
			FullVertex{
					   .position = triangle[2].position,
					   .texcoord = triangle[2].texcoord,
					   .normal = triangle[2].normal,
					   .tangent = encoded_tangent,
					   }
		};
	}

	std::expected<Geometry, Error> Geometry::create(
		std::span<const FullVertex> vertices,
		std::span<const uint32_t> indices
	) noexcept
	{
		if (vertices.empty()) return Error("Invalid primitive with no vertices");
		if (indices.empty()) return Error("Invalid primitive with no indices");
		if (indices.size() % 3 != 0)
			return Error(
				"Invalid primitive with non-triangle indices",
				std::format("Index count: {}", indices.size())
			);

		const auto vertex_count = vertices.size();
		for (const auto [index, vertex_index] : std::views::enumerate(indices))
		{
			if (vertex_index >= vertex_count)
				return Error(
					"Invalid primitive with out-of-bounds index",
					std::format(
						"(index_buffer[{}] = {}) >= (vertex_buffer.size() = {})",
						index,
						vertex_index,
						vertex_count
					)
				);
		}

		const auto min_bound =
			std::ranges::fold_left_first(
				indices | std::views::transform([&](uint32_t index) { return vertices[index].position; }),
				[](const glm::vec3& a, const glm::vec3& b) { return glm::min(a, b); }
			).value_or(glm::vec3(0.0f));

		const auto max_bound =
			std::ranges::fold_left_first(
				indices | std::views::transform([&](uint32_t index) { return vertices[index].position; }),
				[](const glm::vec3& a, const glm::vec3& b) { return glm::max(a, b); }
			).value_or(glm::vec3(0.0f));

		return Geometry(
			std::vector<FullVertex>(vertices.begin(), vertices.end()),
			std::vector<uint32_t>(indices.begin(), indices.end()),
			min_bound,
			max_bound
		);
	}
}

namespace model::util
{
	std::expected<std::vector<uint32_t>, Error> expand_triangle_strip(
		std::span<const uint32_t> indices
	) noexcept
	{
		if (indices.size() < 3)
			return Error(
				"Invalid triangle strip with less than 3 indices",
				std::format("Index count: {}", indices.size())
			);

		const auto triangle_count = indices.size() - 2;

		return std::views::iota(0zu, triangle_count)
			| std::views::transform([&indices](size_t i) {
				   const auto i0 = indices[i];
				   const auto i1 = indices[i + 1 + (i % 2)];
				   const auto i2 = indices[i + 2 - (i % 2)];
				   return std::to_array({i0, i1, i2});
			   })
			| std::views::join
			| std::ranges::to<std::vector>();
	}

	std::expected<std::vector<uint32_t>, Error> expand_triangle_fan(
		std::span<const uint32_t> indices
	) noexcept
	{
		if (indices.size() < 3)
			return Error(
				"Invalid triangle fan with less than 3 indices",
				std::format("Index count: {}", indices.size())
			);

		const auto triangle_count = indices.size() - 2;

		return std::views::iota(0zu, triangle_count)
			| std::views::transform([&indices](size_t i) {
				   const auto i0 = indices[i + 1];
				   const auto i1 = indices[i + 2];
				   const auto i2 = indices[0];
				   return std::to_array({i0, i1, i2});
			   })
			| std::views::join
			| std::ranges::to<std::vector>();
	}
}

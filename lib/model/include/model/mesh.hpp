#pragma once

#include "common/util/arithmetic-functor.hpp"
#include "common/util/error.hpp"
#include <glm/glm.hpp>
#include <optional>
#include <vulkan/vulkan.hpp>

namespace model
{
	///
	/// @brief Vertex structure types
	///
	///
	enum class VertexType
	{
		Minimum,     // Minimum vertex, only position and texcoord
		NormalOnly,  // Normal only vertex, position, texcoord and normal
		Full,        // Full vertex, contains position, texcoord, normal and tangent
	};

	///
	/// @brief Vertex structure template, specialized for different vertex types
	///
	/// @tparam Type Vertex type
	///
	template <VertexType Type>
	struct Vertex;

	using MinimumVertex = Vertex<VertexType::Minimum>;
	using NormalOnlyVertex = Vertex<VertexType::NormalOnly>;
	using FullVertex = Vertex<VertexType::Full>;

	template <>
	struct Vertex<VertexType::Minimum>
	{
		glm::vec3 position;
		glm::vec2 texcoord;

		///
		/// @brief Construct normals from a triangle of minimum vertices
		/// @note If encountered degenerated triangle, normal is set to zero
		///
		/// @param triangle Triangle of minimum vertices
		/// @return Triangle of normal only vertices
		///
		[[nodiscard]]
		static std::array<NormalOnlyVertex, 3> construct_normal(
			const std::array<MinimumVertex, 3>& triangle
		) noexcept;
	};

	template <>
	struct Vertex<VertexType::NormalOnly>
	{
		glm::vec3 position;
		glm::vec2 texcoord;
		glm::vec3 normal;

		///
		/// @brief Construct a normal only vertex from a minimum vertex
		///
		/// @param vertex Minimum vertex
		/// @param normal Normal vector
		/// @return Normal only vertex
		///
		static Vertex from(const Vertex<VertexType::Minimum>& vertex, glm::vec3 normal) noexcept
		{
			return {.position = vertex.position, .texcoord = vertex.texcoord, .normal = normal};
		}

		///
		/// @brief Construct tangents from a triangle of normal only vertices
		/// @note If encountered degenerated triangle, tangent is set to zero
		///
		/// @param triangle Triangle of normal only vertices
		/// @return Triangle of full vertices
		///
		[[nodiscard]]
		static std::array<FullVertex, 3> construct_tangent(
			const std::array<NormalOnlyVertex, 3>& triangle
		) noexcept;
	};

	template <>
	struct Vertex<VertexType::Full>
	{
		glm::vec3 position;
		glm::vec2 texcoord;
		glm::vec3 normal;

		///
		/// @brief Encoded tangent vector
		/// @details `w` component indicates the handedness of the tangent space, should be either `1.0` or
		/// `-1.0`. `1.0` indicates right-handed tangent space (Vulkan default), while `-1.0` indicates
		/// left-handed tangent space.
		///
		glm::vec4 tangent;

		///
		/// @brief Construct a full vertex from a normal only vertex
		///
		/// @param vertex Normal only vertex
		/// @param tangent Tangent vector, with w component indicating the handedness of the tangent space
		/// @return Full vertex
		///
		static Vertex from(const Vertex<VertexType::NormalOnly>& vertex, glm::vec4 tangent) noexcept
		{
			return {
				.position = vertex.position,
				.texcoord = vertex.texcoord,
				.normal = vertex.normal,
				.tangent = tangent
			};
		}

		///
		/// @brief Construct a full vertex from a minimum vertex
		///
		/// @param vertex Minimum vertex
		/// @param normal Normal vector
		/// @param tangent Tangent vector, with w component indicating the handedness of the tangent space
		/// @return Full vertex
		///
		static Vertex from(
			const Vertex<VertexType::Minimum>& vertex,
			glm::vec3 normal,
			glm::vec4 tangent
		) noexcept
		{
			return {
				.position = vertex.position,
				.texcoord = vertex.texcoord,
				.normal = normal,
				.tangent = tangent
			};
		}

		///
		/// @brief Get a normal only vertex from a full vertex, discarding the tangent
		///
		/// @return Normal only vertex
		///
		[[nodiscard]]
		NormalOnlyVertex to_normal_only() const noexcept
		{
			return {.position = position, .texcoord = texcoord, .normal = normal};
		}

		///
		/// @brief Get a minimum vertex from a full vertex, discarding the normal and tangent
		///
		/// @return Minimum vertex
		///
		[[nodiscard]]
		MinimumVertex to_minimum() const noexcept
		{
			return {.position = position, .texcoord = texcoord};
		}
	};

	///
	/// @brief Geometry object, describes a triangle-list geometry without material
	/// @note Visit its members through `operator->`
	///
	class Geometry
	{
	  public:

		std::vector<FullVertex> vertices;
		std::vector<uint32_t> indices;
		glm::vec3 aabb_min, aabb_max;

		// PLANNED: Support LOD generation
		[[nodiscard]]
		Geometry simplify() const noexcept;

		///
		/// @brief Create and verify a primitive from vertex and index data
		///
		/// @param vertices Input vertices
		/// @param indices Input indices. All indices should be a valid index referencing an element in
		/// vertices, or verification will fail
		/// @return Verified geometry, or error
		///
		[[nodiscard]]
		static std::expected<Geometry, Error> create(
			std::span<const FullVertex> vertices,
			std::span<const uint32_t> indices
		) noexcept;

	  private:

		explicit Geometry(
			std::vector<FullVertex> vertices,
			std::vector<uint32_t> indices,
			glm::vec3 min_bound,
			glm::vec3 max_bound
		) noexcept :
			vertices(std::move(vertices)),
			indices(std::move(indices)),
			aabb_min(min_bound),
			aabb_max(max_bound)
		{}

	  public:

		Geometry(const Geometry&) = default;
		Geometry(Geometry&&) = default;
		Geometry& operator=(const Geometry&) = default;
		Geometry& operator=(Geometry&&) = default;
	};

	///
	/// @brief Primitive object, describes a geometry with material reference
	/// @note Implementation need not verify the material index, as it will be verified together with the
	/// material set.
	/// @warning Do not trust the material index before it is verified
	///
	struct Primitive
	{
		Geometry geometry;

		// PLANNED: Lod levels

		///
		/// @brief Optional material index
		/// @details When missing, use a default material, which can be obtained by
		/// `model::MaterialList::default_material`
		///
		///
		std::optional<uint32_t> material_index;
	};

	///
	/// @brief Mesh object, contains a collection of primitives
	///
	struct Mesh
	{
		std::vector<Primitive> primitives;
	};

	namespace util
	{
		///
		/// @brief Expand triangle strip indices to triangle list indices
		///
		/// @note This function follows glTF 2.0 spec for triangle strip expansion. See
		/// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview
		///
		/// @warning This function does not perform primitive restart handling, the input indices should not
		/// contain primitive restart index.
		///
		/// @param indices Triangle strip indices
		/// @return Expanded triangle list indices, or an Error if the input is invalid
		///
		std::expected<std::vector<uint32_t>, Error> expand_triangle_strip(
			std::span<const uint32_t> indices
		) noexcept;

		///
		/// @brief Expand triangle fan indices to triangle list indices
		///
		/// @note This function follows glTF 2.0 spec for triangle fan expansion. See
		/// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#meshes-overview
		///
		/// @warning This function does not perform primitive restart handling, the input indices should not
		/// contain primitive restart index.
		///
		/// @param indices Triangle fan indices
		/// @return Expanded triangle list indices, or an Error if the input is invalid
		///
		std::expected<std::vector<uint32_t>, Error> expand_triangle_fan(
			std::span<const uint32_t> indices
		) noexcept;

		///
		/// @brief Expand indexed geometry to non-indexed geometry by expanding the indices to vertices
		///
		/// @tparam T Vertex type
		/// @param indices Input indices. All indices should be a valid index referencing an element in
		/// vertices, or an error will be returned
		/// @param vertices Input vertices
		/// @return Expanded vertices, or an Error if the input is invalid
		///
		template <typename T>
		std::expected<std::vector<T>, Error> expand_indices(
			std::span<const uint32_t> indices,
			std::span<const T> vertices
		) noexcept
		{
			if (!std::ranges::all_of(indices, ::util::LessThanValue(vertices.size())))
				return Error("Invalid index found");

			return indices
				| std::views::transform([&](uint32_t idx) { return vertices[idx]; })
				| std::ranges::to<std::vector>();
		}
	}
}

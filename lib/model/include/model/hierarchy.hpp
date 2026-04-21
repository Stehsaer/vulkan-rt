#pragma once

#include "common/util/error.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory_resource>
#include <optional>
#include <ranges>
#include <utility>

namespace model
{
	///
	/// @brief Node transformation
	///
	///
	struct Transform
	{
		glm::vec3 scale = glm::vec3(1.0f);
		glm::quat rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
		glm::vec3 translation = glm::vec3(0.0f);

		///
		/// @brief Convert to transformation matrix
		///
		/// @return 4x4 transformation matrix
		///
		[[nodiscard]]
		glm::mat4 to_matrix() const noexcept;
	};

	///
	/// @brief Node data without hierarchy info
	///
	///
	struct NodeData
	{
		Transform transform = {};
		std::optional<uint32_t> mesh_index = std::nullopt;
	};

	///
	/// @brief Node type
	///
	///
	enum class NodeType
	{
		ParentOnly,  // See `ParentOnlyNode`
		ChildOnly,   // See `ChildOnlyNode`
		Full,        // See `FullNode`
	};

	///
	/// @brief A node
	///
	/// @tparam Type Node type
	///
	template <NodeType Type>
	struct Node;

	///
	/// @brief A node with parent index only
	///
	///
	using ParentOnlyNode = Node<NodeType::ParentOnly>;

	///
	/// @brief A node with child indices only
	///
	///
	using ChildOnlyNode = Node<NodeType::ChildOnly>;

	///
	/// @brief A node with both parent index and child indices
	///
	///
	using FullNode = Node<NodeType::Full>;

	template <>
	struct Node<NodeType::ParentOnly>
	{
		std::optional<uint32_t> parent_index;
		NodeData data = {};
	};

	template <>
	struct Node<NodeType::ChildOnly>
	{
		std::vector<uint32_t> child_indices;
		NodeData data = {};
	};

	template <>
	struct Node<NodeType::Full>
	{
		std::optional<uint32_t> parent_index;
		std::vector<uint32_t> child_indices;
		NodeData data = {};
	};

	///
	/// @brief Hierarchy class, contains a tree of nodes
	/// @note Access the nodes and precomputed data using `operator->`
	///
	class Hierarchy
	{
	  public:

		///
		/// @brief Create a hierarchy from a list of nodes with arbitrary parent/child index storage
		///
		/// @tparam Type Type of nodes
		/// @param nodes List of nodes to construct the hierarchy from. The function will verify that the
		/// parent/child indices are valid and consistent, and will construct the missing parent/child indices
		/// if necessary.
		/// @return Created `Hierarchy` if successful, or an `Error` if validation fails
		///
		template <NodeType Type>
		[[nodiscard]]
		static std::expected<Hierarchy, Error> create(std::span<const Node<Type>> nodes) noexcept;

		///
		/// @brief Create a hierarchy from a contiguous range of nodes with arbitrary parent/child index
		/// storage
		///
		/// @tparam Range Range type
		/// @param std::convertible_to<std::ranges::range_value_t<Range>, ChildOnlyNode>
		/// @return
		///
		template <std::ranges::input_range Range>
			requires(std::ranges::contiguous_range<Range>)
		[[nodiscard]]
		static std::expected<Hierarchy, Error> create(Range&& range) noexcept
		{
			using RangeValueType = std::add_const_t<std::ranges::range_value_t<Range>>;

			static_assert(
				std::same_as<std::remove_const_t<RangeValueType>, ParentOnlyNode>
					|| std::same_as<std::remove_const_t<RangeValueType>, ChildOnlyNode>,
				"Elements of input range must be one of ParentOnlyNode or ChildOnlyNode"
			);

			return create(std::span<RangeValueType>(range));
		}

		///
		/// @brief Drawcall, containing the node index and a mesh index
		///
		///
		struct Drawcall
		{
			uint32_t node_index;
			uint32_t mesh_index;
		};

		///
		/// @brief Compute absolute transforms of all nodes
		///
		/// @param root_transform Transform applied to the root node in addition to its own transform
		/// @param memory_resource Memory resource to use for allocating the output vector, defaulting to the
		/// new/delete memory resource
		/// @return List of transform 4x4 matrices, one-to-one corresponding to the nodes
		///
		[[nodiscard]]
		std::pmr::vector<glm::mat4> compute_transforms(
			const glm::mat4& root_transform,
			std::pmr::memory_resource& memory_resource = *std::pmr::new_delete_resource()
		) const noexcept;

		///
		/// @brief Get renderable nodes in the hierarchy
		///
		/// @param memory_resource Memory resource to use for allocating the output vector, defaulting to the
		/// new/delete memory resource
		/// @return List of renderable nodes, each containing the node index and a mesh index
		///
		[[nodiscard]]
		std::pmr::vector<Drawcall> get_drawcalls(
			std::pmr::memory_resource& memory_resource = *std::pmr::new_delete_resource()
		) const noexcept;

		///
		/// @brief Get the nodes in the hierarchy as a list of `FullNode`
		///
		/// @return List of nodes in the hierarchy, each with both parent index and child indices
		///
		[[nodiscard]]
		std::span<const FullNode> get_nodes() const noexcept
		{
			return nodes;
		}

	  private:

		std::vector<FullNode> nodes;
		std::vector<uint32_t> bfs_indices;
		std::vector<Drawcall> renderable_nodes;

		explicit Hierarchy(
			std::vector<FullNode> nodes,
			std::vector<uint32_t> bfs_indices,
			std::vector<Drawcall> renderable_nodes
		) :
			nodes(std::move(nodes)),
			bfs_indices(std::move(bfs_indices)),
			renderable_nodes(std::move(renderable_nodes))
		{}

		static std::vector<Drawcall> find_renderable_nodes(
			const std::vector<FullNode>& double_linked_nodes
		) noexcept;

	  public:

		Hierarchy(const Hierarchy&) = default;
		Hierarchy(Hierarchy&&) = default;
		Hierarchy& operator=(const Hierarchy&) = default;
		Hierarchy& operator=(Hierarchy&&) = default;
	};
}

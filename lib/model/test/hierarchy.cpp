#include "model/hierarchy.hpp"
#include "common/test-macro.hpp"

#include <doctest.h>
#include <glm/gtx/matrix_decompose.hpp>
#include <tuple>
#include <vector>

TEST_SUITE("Normal Hierarchy Creation")
{
	TEST_CASE("Parent Only")
	{
		SUBCASE("1")
		{
			const std::vector<model::ParentOnlyNode> nodes = {
				{.parent_index = std::nullopt},
				{.parent_index = 0},
				{.parent_index = 0},
				{.parent_index = 1},
				{.parent_index = 1},
				{.parent_index = 2}
			};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_SUCCESS(hierarchy_result);
			const auto hierarchy = std::move(*hierarchy_result);

			CHECK_EQ(hierarchy.get_nodes().size(), 6);

			CHECK_EQ(hierarchy.get_nodes()[0].parent_index, std::nullopt);
			CHECK_EQ(hierarchy.get_nodes()[1].parent_index, 0);
			CHECK_EQ(hierarchy.get_nodes()[2].parent_index, 0);
			CHECK_EQ(hierarchy.get_nodes()[3].parent_index, 1);
			CHECK_EQ(hierarchy.get_nodes()[4].parent_index, 1);
			CHECK_EQ(hierarchy.get_nodes()[5].parent_index, 2);

			CHECK_EQ(hierarchy.get_nodes()[0].child_indices, std::vector<uint32_t>{1, 2});
			CHECK_EQ(hierarchy.get_nodes()[1].child_indices, std::vector<uint32_t>{3, 4});
			CHECK_EQ(hierarchy.get_nodes()[2].child_indices, std::vector<uint32_t>{5});
			CHECK(hierarchy.get_nodes()[3].child_indices.empty());
			CHECK(hierarchy.get_nodes()[4].child_indices.empty());
			CHECK(hierarchy.get_nodes()[5].child_indices.empty());
		}

		SUBCASE("2")
		{
			const std::vector<model::ParentOnlyNode> nodes = {
				{.parent_index = 3},
				{.parent_index = 3},
				{.parent_index = 0},
				{.parent_index = std::nullopt},
				{.parent_index = 0},
				{.parent_index = 1}
			};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_SUCCESS(hierarchy_result);
			const auto hierarchy = std::move(*hierarchy_result);

			CHECK_EQ(hierarchy.get_nodes().size(), 6);

			CHECK_EQ(hierarchy.get_nodes()[0].parent_index, 3);
			CHECK_EQ(hierarchy.get_nodes()[1].parent_index, 3);
			CHECK_EQ(hierarchy.get_nodes()[2].parent_index, 0);
			CHECK_EQ(hierarchy.get_nodes()[3].parent_index, std::nullopt);
			CHECK_EQ(hierarchy.get_nodes()[4].parent_index, 0);
			CHECK_EQ(hierarchy.get_nodes()[5].parent_index, 1);

			CHECK_EQ(hierarchy.get_nodes()[0].child_indices, std::vector<uint32_t>{2, 4});
			CHECK_EQ(hierarchy.get_nodes()[1].child_indices, std::vector<uint32_t>{5});
			CHECK(hierarchy.get_nodes()[2].child_indices.empty());
			CHECK_EQ(hierarchy.get_nodes()[3].child_indices, std::vector<uint32_t>{0, 1});
			CHECK(hierarchy.get_nodes()[4].child_indices.empty());
			CHECK(hierarchy.get_nodes()[5].child_indices.empty());
		}
	}

	TEST_CASE("Children only")
	{
		const std::vector<model::ChildOnlyNode> nodes = {
			{.child_indices = {1, 2}},
			{.child_indices = {3, 4}},
			{.child_indices = {5}},
			{.child_indices = {}},
			{.child_indices = {}},
			{.child_indices = {}}
		};

		auto hierarchy_result = model::Hierarchy::create(nodes);
		EXPECT_SUCCESS(hierarchy_result);
		const auto hierarchy = std::move(*hierarchy_result);

		CHECK_EQ(hierarchy.get_nodes().size(), 6);

		CHECK_EQ(hierarchy.get_nodes()[0].parent_index, std::nullopt);
		CHECK_EQ(hierarchy.get_nodes()[1].parent_index, 0);
		CHECK_EQ(hierarchy.get_nodes()[2].parent_index, 0);
		CHECK_EQ(hierarchy.get_nodes()[3].parent_index, 1);
		CHECK_EQ(hierarchy.get_nodes()[4].parent_index, 1);
		CHECK_EQ(hierarchy.get_nodes()[5].parent_index, 2);

		CHECK_EQ(hierarchy.get_nodes()[0].child_indices, std::vector<uint32_t>{1, 2});
		CHECK_EQ(hierarchy.get_nodes()[1].child_indices, std::vector<uint32_t>{3, 4});
		CHECK_EQ(hierarchy.get_nodes()[2].child_indices, std::vector<uint32_t>{5});
		CHECK(hierarchy.get_nodes()[3].child_indices.empty());
		CHECK(hierarchy.get_nodes()[4].child_indices.empty());
		CHECK(hierarchy.get_nodes()[5].child_indices.empty());
	}
}

TEST_SUITE("Abnormal Hierarchy Creation")
{
	TEST_CASE("Empty")
	{
		SUBCASE("Parent only")
		{
			const std::vector<model::ParentOnlyNode> nodes;
			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}

		SUBCASE("Children only")
		{
			const std::vector<model::ChildOnlyNode> nodes;
			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}
	}

	TEST_CASE("Multiple root nodes")
	{
		SUBCASE("Parent only")
		{
			const std::vector<model::ParentOnlyNode> nodes =
				{{.parent_index = std::nullopt}, {.parent_index = std::nullopt}};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}

		SUBCASE("Children only")
		{
			const std::vector<model::ChildOnlyNode> nodes = {{.child_indices = {}}, {.child_indices = {}}};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}
	}

	TEST_CASE("Cycle 1")
	{
		SUBCASE("Parent only")
		{
			const std::vector<model::ParentOnlyNode> nodes = {{.parent_index = 1}, {.parent_index = 0}};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}

		SUBCASE("Children only")
		{
			const std::vector<model::ChildOnlyNode> nodes = {{.child_indices = {1}}, {.child_indices = {0}}};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}
	}

	TEST_CASE("Cycle 2")
	{
		SUBCASE("Parent only")
		{
			const std::vector<model::ParentOnlyNode> nodes = {
				{.parent_index = 6},
				{.parent_index = 0},
				{.parent_index = 1},
				{.parent_index = 2},
				{.parent_index = 3},
				{.parent_index = 4},
				{.parent_index = 5}
			};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}

		SUBCASE("Children only")
		{
			const std::vector<model::ChildOnlyNode> nodes = {
				{.child_indices = {6}},
				{.child_indices = {0}},
				{.child_indices = {1}},
				{.child_indices = {2}},
				{.child_indices = {3}},
				{.child_indices = {4}},
				{.child_indices = {5}}
			};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}
	}

	TEST_CASE("Detached cycle")
	{
		SUBCASE("Parent only")
		{
			const std::vector<model::ParentOnlyNode> nodes = {
				{.parent_index = std::nullopt},
				{.parent_index = 0},
				{.parent_index = 1},
				{.parent_index = 2},
				{.parent_index = 5},
				{.parent_index = 6},
				{.parent_index = 4}
			};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}

		SUBCASE("Children only")
		{
			const std::vector<model::ChildOnlyNode> nodes = {
				{.child_indices = {1}},
				{.child_indices = {2}},
				{.child_indices = {3}},
				{.child_indices = {}},
				{.child_indices = {5}},
				{.child_indices = {6}},
				{.child_indices = {4}}
			};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}
	}

	TEST_CASE("Self reference")
	{
		SUBCASE("Parent only")
		{
			const std::vector<model::ParentOnlyNode> nodes = {{.parent_index = 1}, {.parent_index = 1}};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}

		SUBCASE("Child only")
		{
			const std::vector<model::ChildOnlyNode> nodes =
				{{.child_indices = {1}}, {.child_indices = {1, 2}}, {.child_indices = {}}};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}
	}

	TEST_CASE("Invalid parent index")
	{
		SUBCASE("Parent only")
		{
			const std::vector<model::ParentOnlyNode> nodes =
				{{.parent_index = std::nullopt}, {.parent_index = 5}};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}

		SUBCASE("Child only")
		{
			const std::vector<model::ChildOnlyNode> nodes = {{.child_indices = {5}}, {.child_indices = {}}};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}
	}

	TEST_CASE("Invalid child index")
	{
		SUBCASE("Parent only")
		{
			const std::vector<model::ParentOnlyNode> nodes =
				{{.parent_index = std::nullopt}, {.parent_index = 0}};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_SUCCESS(hierarchy_result);
			auto hierarchy = std::move(*hierarchy_result);

			const auto transforms = hierarchy.compute_transforms(glm::mat4(1.0));
			CHECK_EQ(transforms.size(), 2);
		}

		SUBCASE("Child only")
		{
			const std::vector<model::ChildOnlyNode> nodes = {{.child_indices = {1}}, {.child_indices = {}}};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_SUCCESS(hierarchy_result);
			auto hierarchy = std::move(*hierarchy_result);

			const auto transforms = hierarchy.compute_transforms(glm::mat4(1.0));
			CHECK_EQ(transforms.size(), 2);
		}
	}

	TEST_CASE("Deep hierarchy")
	{
		SUBCASE("Parent only")
		{
			const auto nodes =
				std::views::iota(0zu, 10zu)
				| std::views::transform([](size_t i) {
					  return model::ParentOnlyNode{
						  .parent_index = i == 0 ? std::nullopt : std::optional<uint32_t>(i - 1)
					  };
				  })
				| std::ranges::to<std::vector>();

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_SUCCESS(hierarchy_result);
			const auto hierarchy = std::move(*hierarchy_result);

			CHECK_EQ(hierarchy.get_nodes().size(), 10);

			for (const auto i : std::views::iota(0u, 10u))
			{
				if (i == 0)
					CHECK_FALSE(hierarchy.get_nodes()[i].parent_index.has_value());
				else
					CHECK_EQ(hierarchy.get_nodes()[i].parent_index, i - 1);
			}
		}

		SUBCASE("Child only")
		{
			const auto nodes =
				std::views::iota(0u, 10u)
				| std::views::transform([](unsigned i) {
					  return model::ChildOnlyNode{
						  .child_indices = i < 9 ? std::vector<uint32_t>{i + 1} : std::vector<uint32_t>{}
					  };
				  })
				| std::ranges::to<std::vector>();

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_SUCCESS(hierarchy_result);
			const auto hierarchy = std::move(*hierarchy_result);

			CHECK_EQ(hierarchy.get_nodes().size(), 10);
		}
	}

	TEST_CASE("Wide hierarchy")
	{
		SUBCASE("Parent only")
		{
			const auto nodes =
				std::views::iota(0zu, 21zu)
				| std::views::transform([](size_t i) {
					  return model::ParentOnlyNode{
						  .parent_index = i == 0 ? std::nullopt : std::optional<uint32_t>(0)
					  };
				  })
				| std::ranges::to<std::vector>();

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_SUCCESS(hierarchy_result);
			const auto hierarchy = std::move(*hierarchy_result);

			CHECK_EQ(hierarchy.get_nodes().size(), 21);
			CHECK_EQ(hierarchy.get_nodes()[0].child_indices.size(), 20);
		}

		SUBCASE("Child only")
		{
			std::vector<model::ChildOnlyNode> nodes;
			nodes.reserve(21);

			const auto child_indices = std::views::iota(1u, 21u) | std::ranges::to<std::vector>();
			nodes.push_back({.child_indices = child_indices});

			nodes.append_range(
				std::views::iota(0u, 20u)
				| std::views::transform([](uint32_t) { return model::ChildOnlyNode{.child_indices = {}}; })
			);

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_SUCCESS(hierarchy_result);
			const auto hierarchy = std::move(*hierarchy_result);

			CHECK_EQ(hierarchy.get_nodes().size(), 21);
			CHECK_EQ(hierarchy.get_nodes()[0].child_indices.size(), 20);
		}
	}

	TEST_CASE("Complex tree structure")
	{
		SUBCASE("Parent only")
		{
			const std::vector<model::ParentOnlyNode> nodes = {
				{.parent_index = std::nullopt},
				{.parent_index = 0},
				{.parent_index = 0},
				{.parent_index = 0},
				{.parent_index = 1},
				{.parent_index = 1},
				{.parent_index = 2},
				{.parent_index = 2},
				{.parent_index = 3},
				{.parent_index = 4},
				{.parent_index = 5},
				{.parent_index = 6}
			};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_SUCCESS(hierarchy_result);
			const auto hierarchy = std::move(*hierarchy_result);

			CHECK_EQ(hierarchy.get_nodes().size(), 12);

			CHECK_EQ(hierarchy.get_nodes()[0].child_indices.size(), 3);
			CHECK_EQ(hierarchy.get_nodes()[1].child_indices.size(), 2);
			CHECK_EQ(hierarchy.get_nodes()[2].child_indices.size(), 2);
			CHECK_EQ(hierarchy.get_nodes()[3].child_indices.size(), 1);
		}

		SUBCASE("Child only")
		{
			const std::vector<model::ChildOnlyNode> nodes = {
				{.child_indices = {1, 2, 3}},
				{.child_indices = {4, 5}},
				{.child_indices = {6, 7}},
				{.child_indices = {8}},
				{.child_indices = {9}},
				{.child_indices = {10}},
				{.child_indices = {11}},
				{.child_indices = {}},
				{.child_indices = {}},
				{.child_indices = {}},
				{.child_indices = {}},
				{.child_indices = {}}
			};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_SUCCESS(hierarchy_result);
			const auto hierarchy = std::move(*hierarchy_result);

			CHECK_EQ(hierarchy.get_nodes().size(), 12);

			CHECK_EQ(hierarchy.get_nodes()[0].child_indices.size(), 3);
			CHECK_EQ(hierarchy.get_nodes()[1].child_indices.size(), 2);
			CHECK_EQ(hierarchy.get_nodes()[2].child_indices.size(), 2);
			CHECK_EQ(hierarchy.get_nodes()[3].child_indices.size(), 1);
		}
	}

	TEST_CASE("Disconnected nodes")
	{
		SUBCASE("Parent only")
		{
			const std::vector<model::ParentOnlyNode> nodes =
				{{.parent_index = std::nullopt}, {.parent_index = 100}};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}

		SUBCASE("Child only")
		{
			const std::vector<model::ChildOnlyNode> nodes = {{.child_indices = {100}}, {.child_indices = {}}};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_FAIL(hierarchy_result);
		}
	}

	TEST_CASE("Single node")
	{
		SUBCASE("Parent only")
		{
			const std::vector<model::ParentOnlyNode> nodes = {{.parent_index = std::nullopt}};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_SUCCESS(hierarchy_result);
			const auto hierarchy = std::move(*hierarchy_result);

			CHECK_EQ(hierarchy.get_nodes().size(), 1);
			CHECK_FALSE(hierarchy.get_nodes()[0].parent_index.has_value());
			CHECK(hierarchy.get_nodes()[0].child_indices.empty());
		}

		SUBCASE("Child only")
		{
			const std::vector<model::ChildOnlyNode> nodes = {{.child_indices = {}}};

			auto hierarchy_result = model::Hierarchy::create(nodes);
			EXPECT_SUCCESS(hierarchy_result);
			const auto hierarchy = std::move(*hierarchy_result);

			CHECK_EQ(hierarchy.get_nodes().size(), 1);
			CHECK_FALSE(hierarchy.get_nodes()[0].parent_index.has_value());
			CHECK(hierarchy.get_nodes()[0].child_indices.empty());
		}
	}
}

static std::tuple<glm::vec3, glm::quat, glm::vec3> decompose(const glm::mat4& mat) noexcept
{
	glm::vec3 translation;
	glm::quat rotation;
	glm::vec3 scale;
	glm::vec3 skew;
	glm::vec4 perspective;

	glm::decompose(mat, scale, rotation, translation, skew, perspective);

	return {translation, rotation, scale};
}

static bool check_matrix(
	const glm::mat4& mat,
	glm::vec3 expected_translation,
	glm::quat expected_rotation,
	glm::vec3 expected_scale
) noexcept
{
	const auto [translation, rotation, scale] = decompose(mat);

	const bool match_translation = glm::all(glm::epsilonEqual(translation, expected_translation, 1.0e-4f));
	const bool match_scale = glm::all(glm::epsilonEqual(scale, expected_scale, 1.0e-4f));
	const bool match_rotation = glm::all(glm::epsilonEqual(rotation, expected_rotation, 1.0e-4f));

	return match_translation && match_scale && match_rotation;
}

TEST_SUITE("Transformation")
{
	TEST_CASE("Simple 1")
	{
		/* [2]
		 *  | \
		 * [1] [3]
		 *  |
		 * [0]
		 *
		 * [2]: scale 2.0
		 * [1]: scale 3.0
		 * [0]: translate (1.0, 0.0, 0.0)
		 * [3]: translate (0.0, 1.0, 0.0)
		 */

		const std::vector<model::ParentOnlyNode> nodes = {
			{.parent_index = 1,            .data = {.transform = {.translation = {1.0, 0.0, 0.0}}}},
			{.parent_index = 2,            .data = {.transform = {.scale = {3.0, 3.0, 3.0}}}      },
			{.parent_index = std::nullopt, .data = {.transform = {.scale = {2.0, 2.0, 2.0}}}      },
			{.parent_index = 2,            .data = {.transform = {.translation = {0.0, 1.0, 0.0}}}}
		};

		auto hierarchy_result = model::Hierarchy::create(nodes);
		EXPECT_SUCCESS(hierarchy_result);
		auto hierarchy = std::move(*hierarchy_result);

		const auto transforms = hierarchy.compute_transforms(glm::mat4(1.0));

		constexpr glm::quat unit_quat = {1.0, 0.0, 0.0, 0.0};

		CHECK(check_matrix(transforms[2], glm::vec3(0.0), unit_quat, glm::vec3(2.0)));
		CHECK(check_matrix(transforms[1], glm::vec3(0.0), unit_quat, glm::vec3(6.0)));
		CHECK(check_matrix(transforms[0], glm::vec3(6.0, 0.0, 0.0), unit_quat, glm::vec3(6.0)));
		CHECK(check_matrix(transforms[3], glm::vec3(0.0, 2.0, 0.0), unit_quat, glm::vec3(2.0)));
	}

	TEST_CASE("Simple 2")
	{
		/* [0] (root)
		 *  | \
		 * [1] [2]
		 *
		 * [0]: rotate 90° around Z-axis
		 * [1]: translate (1, 0, 0)
		 * [2]: scale (0.5, 0.5, 0.5)
		 */

		const glm::quat rot_z_90 = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.0f, 0.0f, 1.0f));

		const std::vector<model::ParentOnlyNode> nodes = {
			{.parent_index = std::nullopt, .data = {.transform = {.rotation = rot_z_90}}          },
			{.parent_index = 0,            .data = {.transform = {.translation = {1.0, 0.0, 0.0}}}},
			{.parent_index = 0,            .data = {.transform = {.scale = {0.5, 0.5, 0.5}}}      }
		};

		auto hierarchy_result = model::Hierarchy::create(nodes);
		EXPECT_SUCCESS(hierarchy_result);
		auto hierarchy = std::move(*hierarchy_result);

		const auto transforms = hierarchy.compute_transforms(glm::mat4(1.0));

		// [0]: pure rotation
		// [1]: rotate then translate (1,0,0) -> absolute translation = (0,1,0)
		// [2]: rotate then scale -> absolute scale = (0.5, 0.5, 0.5)
		CHECK(check_matrix(transforms[0], glm::vec3(0.0), rot_z_90, glm::vec3(1.0)));
		CHECK(check_matrix(transforms[1], glm::vec3(0.0, 1.0, 0.0), rot_z_90, glm::vec3(1.0)));
		CHECK(check_matrix(transforms[2], glm::vec3(0.0), rot_z_90, glm::vec3(0.5)));
	}
}

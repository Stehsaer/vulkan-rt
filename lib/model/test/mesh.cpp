#include "model/mesh.hpp"
#include "common/test-macro.hpp"

#include <doctest.h>
#include <glm/gtc/epsilon.hpp>

TEST_CASE("Normal Generation")
{
	const model::MinimumVertex v0{
		.position = {0.0f, 0.0f, 0.0f},
		.texcoord = {0.0f, 0.0f}
	};

	const model::MinimumVertex v1{
		.position = {1.0f, 0.0f, 0.0f},
		.texcoord = {1.0f, 0.0f}
	};

	const model::MinimumVertex v2{
		.position = {0.0f, 1.0f, 0.0f},
		.texcoord = {0.0f, 1.0f}
	};

	const auto triangle = std::array{v0, v1, v2};
	const auto normal_triangle = model::MinimumVertex::construct_normal(triangle);

	CHECK(glm::all(glm::epsilonEqual(normal_triangle[0].normal, glm::vec3(0.0f, 0.0f, 1.0f), 0.0001f)));
	CHECK(glm::all(glm::epsilonEqual(normal_triangle[1].normal, glm::vec3(0.0f, 0.0f, 1.0f), 0.0001f)));
	CHECK(glm::all(glm::epsilonEqual(normal_triangle[2].normal, glm::vec3(0.0f, 0.0f, 1.0f), 0.0001f)));
}

TEST_CASE("Degenerate Normal Generation")
{
	const model::MinimumVertex v0{
		.position = {0.0f, 0.0f, 0.0f},
		.texcoord = {0.0f, 0.0f}
	};

	const model::MinimumVertex v1{
		.position = {0.0f, 0.0f, 0.0f},
		.texcoord = {1.0f, 0.0f}
	};

	const model::MinimumVertex v2{
		.position = {0.0f, 0.0f, 0.0f},
		.texcoord = {0.0f, 1.0f}
	};

	const auto triangle = std::array{v0, v1, v2};
	const auto normal_triangle = model::MinimumVertex::construct_normal(triangle);

	CHECK(glm::all(glm::epsilonEqual(normal_triangle[0].normal, glm::vec3(0.0f), 0.0001f)));
	CHECK(glm::all(glm::epsilonEqual(normal_triangle[1].normal, glm::vec3(0.0f), 0.0001f)));
	CHECK(glm::all(glm::epsilonEqual(normal_triangle[2].normal, glm::vec3(0.0f), 0.0001f)));
}

TEST_CASE("Tangent Generation")
{
	const model::NormalOnlyVertex v0{
		.position = {0.0f, 0.0f, 0.0f},
		.texcoord = {0.0f, 0.0f},
		.normal = {0.0f, 0.0f, 1.0f}
	};

	const model::NormalOnlyVertex v1{
		.position = {1.0f, 0.0f, 0.0f},
		.texcoord = {1.0f, 0.0f},
		.normal = {0.0f, 0.0f, 1.0f}
	};

	const model::NormalOnlyVertex v2{
		.position = {0.0f, 1.0f, 0.0f},
		.texcoord = {0.0f, 1.0f},
		.normal = {0.0f, 0.0f, 1.0f}
	};

	const auto triangle = std::array{v0, v1, v2};
	const auto full_triangle = model::NormalOnlyVertex::construct_tangent(triangle);

	CHECK(
		glm::all(glm::epsilonEqual(glm::vec3(full_triangle[0].tangent), glm::vec3(1.0f, 0.0f, 0.0f), 0.0001f))
	);
	CHECK(
		glm::all(glm::epsilonEqual(glm::vec3(full_triangle[1].tangent), glm::vec3(1.0f, 0.0f, 0.0f), 0.0001f))
	);
	CHECK(
		glm::all(glm::epsilonEqual(glm::vec3(full_triangle[2].tangent), glm::vec3(1.0f, 0.0f, 0.0f), 0.0001f))
	);
}

TEST_CASE("Degenerate Tangetn Generation")
{
	// pos=(0, 0, 0), texcoord=(0, 0)
	const model::NormalOnlyVertex v0{
		.position = {0.0f, 0.0f, 0.0f},
		.texcoord = {0.0f, 0.0f},
		.normal = {0.0f, 0.0f, 1.0f}
	};

	// pos=(1, 0, 0), texcoord=(1, 0)
	const model::NormalOnlyVertex v1{
		.position = {1.0f, 0.0f, 0.0f},
		.texcoord = {1.0f, 0.0f},
		.normal = {0.0f, 0.0f, 1.0f}
	};

	// pos=(2, 0, 0), texcoord=(2, 0)
	const model::NormalOnlyVertex v2{
		.position = {2.0f, 0.0f, 0.0f},
		.texcoord = {2.0f, 0.0f},
		.normal = {0.0f, 0.0f, 1.0f}
	};

	/*
	 * Note: Impossible to determinte tangent for this triangle since all vertices are colinear in both
	 * position and texcoord space, which results in a degenerate tangent space. The function should return
	 * zero tangent in this case.
	 */

	const auto triangle = std::array{v0, v1, v2};
	const auto full_triangle = model::NormalOnlyVertex::construct_tangent(triangle);

	CHECK(glm::all(glm::epsilonEqual(glm::vec3(full_triangle[0].tangent), glm::vec3(0.0f), 0.0001f)));
	CHECK(glm::all(glm::epsilonEqual(glm::vec3(full_triangle[1].tangent), glm::vec3(0.0f), 0.0001f)));
	CHECK(glm::all(glm::epsilonEqual(glm::vec3(full_triangle[2].tangent), glm::vec3(0.0f), 0.0001f)));
}

TEST_CASE("Geometry Validation")
{
	SUBCASE("Overflow")
	{
		const auto vertices = std::vector<model::FullVertex>(100);
		auto indices = std::vector<uint32_t>(300, 0);
		indices[0] = 100;

		EXPECT_FAIL(model::Geometry::create(vertices, indices));
	}

	SUBCASE("Empty geometry")
	{
		EXPECT_FAIL(model::Geometry::create({}, {}));
	}

	SUBCASE("Not triangle")
	{
		const auto vertices = std::vector<model::FullVertex>(3);
		const auto indices = std::vector<uint32_t>(4, 0);  // Not a multiple of 3
		EXPECT_FAIL(model::Geometry::create(vertices, indices));
	}

	SUBCASE("Valid geometry")
	{
		const std::vector<model::FullVertex> vertices = {
			{.position = {0.0f, 0.0f, 0.0f}, .texcoord = {}, .normal = {}, .tangent = {}},
			{.position = {1.0f, 0.0f, 0.0f}, .texcoord = {}, .normal = {}, .tangent = {}},
			{.position = {0.0f, 1.0f, 0.0f}, .texcoord = {}, .normal = {}, .tangent = {}}
		};
		const auto indices = std::vector<uint32_t>(std::from_range, std::views::iota(0u, 3u));

		auto geometry_result = model::Geometry::create(vertices, indices);
		EXPECT_SUCCESS(geometry_result);
		const auto primitive = std::move(*geometry_result);

		CHECK(glm::all(glm::epsilonEqual(primitive.aabb_min, glm::vec3(0.0f), 0.0001f)));
		CHECK(glm::all(glm::epsilonEqual(primitive.aabb_max, glm::vec3(1.0f, 1.0f, 0.0f), 0.0001f)));
	}
}

TEST_CASE("Indices Expansion")
{
	SUBCASE("Triangle Strip")
	{
		SUBCASE("Invalid input")
		{
			const auto indices = std::vector<uint32_t>{0, 1};  // Not enough indices for a triangle strip
			EXPECT_FAIL(model::util::expand_triangle_strip(indices));
		}

		SUBCASE("Valid input")
		{
			const auto indices = std::vector<uint32_t>{0, 1, 2, 3};  // Two triangles: (0,1,2) and (1,3,2)
			auto result = model::util::expand_triangle_strip(indices);
			EXPECT_SUCCESS(result);
			CHECK_EQ(*result, std::vector<uint32_t>{0, 1, 2, 1, 3, 2});
		}
	}

	SUBCASE("Triangle Fan")
	{
		SUBCASE("Invalid input")
		{
			const auto indices = std::vector<uint32_t>{0, 1};  // Not enough indices for a triangle fan
			EXPECT_FAIL(model::util::expand_triangle_fan(indices));
		}

		SUBCASE("Valid input")
		{
			const auto indices = std::vector<uint32_t>{0, 1, 2, 3};  // Two triangles: (0,1,2) and (0,2,3)
			auto result = model::util::expand_triangle_fan(indices);
			EXPECT_SUCCESS(result);
			CHECK_EQ(*result, std::vector<uint32_t>{1, 2, 0, 2, 3, 0});
		}
	}
}

#include "model/model.hpp"
#include "common/util/error.hpp"
#include "model/hierarchy.hpp"
#include "model/material.hpp"
#include "model/mesh.hpp"

#include "common/test-macro.hpp"
#include <doctest.h>

static model::MaterialList get_valid_material_list() noexcept
{
	const auto texture = std::vector<model::Texture>(
		10,
		model::Texture{
			.source = image::Image<image::Format::Unorm8, image::Layout::RGBA>({1, 1}, {255, 255, 255, 255})
		}
	);

	const auto texture_set =
		model::TextureSet{.albedo = 0, .emissive = 1, .roughness_metallic = 2, .normal = 3};

	const auto material = model::Material{.texture_set = texture_set};

	return model::MaterialList::create(texture, {material}) | Error::unwrap();
}

static model::Geometry get_valid_geometry() noexcept
{
	const std::vector<model::FullVertex> vertices = {
		{.position = {0.0f, 0.0f, 0.0f}, .texcoord = {}, .normal = {}, .tangent = {}},
		{.position = {1.0f, 0.0f, 0.0f}, .texcoord = {}, .normal = {}, .tangent = {}},
		{.position = {0.0f, 1.0f, 0.0f}, .texcoord = {}, .normal = {}, .tangent = {}}
	};
	const auto indices = std::vector<uint32_t>(std::from_range, std::views::iota(0u, 3u));

	return model::Geometry::create(vertices, indices) | Error::unwrap();
}

TEST_CASE("Mesh material index OOB")
{
	auto material_list = get_valid_material_list();

	// OOB at index 1
	auto invalid_mesh =
		model::Mesh{.primitives = {model::Primitive{.geometry = get_valid_geometry(), .material_index = 1}}};

	const std::vector nodes = {
		model::ParentOnlyNode{.parent_index = {}, .data = {}}
	};
	auto valid_hierarchy = model::Hierarchy::create(nodes) | Error::unwrap();

	auto model_result =
		model::Model::assemble(std::move(material_list), {invalid_mesh}, std::move(valid_hierarchy));
	EXPECT_FAIL(model_result);
}

TEST_CASE("Node mesh index OOB")
{
	auto material_list = get_valid_material_list();
	const auto valid_geometry = get_valid_geometry();
	const auto valid_mesh =
		model::Mesh{.primitives = {model::Primitive{.geometry = valid_geometry, .material_index = {}}}};

	SUBCASE("Single node with OOB mesh index")
	{
		// Mesh index 1 is OOB, only 1 mesh exists
		const std::vector nodes = {
			model::ParentOnlyNode{.parent_index = {}, .data = {.mesh_index = 1}}
		};
		auto hierarchy_result = model::Hierarchy::create(nodes);
		EXPECT_SUCCESS(hierarchy_result);
		auto hierarchy = std::move(*hierarchy_result);

		auto model_result =
			model::Model::assemble(std::move(material_list), {valid_mesh}, std::move(hierarchy));
		EXPECT_FAIL(model_result);
	}

	SUBCASE("Multiple nodes with one OOB mesh index")
	{
		// Node 1 has valid mesh index 0, Node 2 has OOB mesh index 5
		const std::vector nodes = {
			model::ParentOnlyNode{.parent_index = {}, .data = {}               },
			model::ParentOnlyNode{.parent_index = 0,  .data = {.mesh_index = 0}},
			model::ParentOnlyNode{.parent_index = 0,  .data = {.mesh_index = 5}}
		};
		auto hierarchy_result = model::Hierarchy::create(nodes);
		EXPECT_SUCCESS(hierarchy_result);
		auto hierarchy = std::move(*hierarchy_result);

		auto model_result =
			model::Model::assemble(std::move(material_list), {valid_mesh}, std::move(hierarchy));
		EXPECT_FAIL(model_result);
	}

	SUBCASE("Deep hierarchy with OOB mesh index")
	{
		// Deep hierarchy, leaf node has OOB mesh index
		const std::vector nodes = {
			model::ParentOnlyNode{.parent_index = {}, .data = {}                },
			model::ParentOnlyNode{.parent_index = 0,  .data = {}                },
			model::ParentOnlyNode{.parent_index = 1,  .data = {}                },
			model::ParentOnlyNode{.parent_index = 2,  .data = {.mesh_index = 10}}
		};
		auto hierarchy_result = model::Hierarchy::create(nodes);
		EXPECT_SUCCESS(hierarchy_result);
		auto hierarchy = std::move(*hierarchy_result);

		auto model_result =
			model::Model::assemble(std::move(material_list), {valid_mesh}, std::move(hierarchy));
		EXPECT_FAIL(model_result);
	}

	SUBCASE("Wide hierarchy with OOB mesh index")
	{
		// Wide hierarchy with multiple children, one has OOB mesh index
		const std::vector nodes = {
			model::ParentOnlyNode{.parent_index = {}, .data = {}               },
			model::ParentOnlyNode{.parent_index = 0,  .data = {.mesh_index = 0}},
			model::ParentOnlyNode{.parent_index = 0,  .data = {.mesh_index = 0}},
			model::ParentOnlyNode{.parent_index = 0,  .data = {.mesh_index = 3}}
		};
		auto hierarchy_result = model::Hierarchy::create(nodes);
		EXPECT_SUCCESS(hierarchy_result);
		auto hierarchy = std::move(*hierarchy_result);

		auto model_result =
			model::Model::assemble(std::move(material_list), {valid_mesh}, std::move(hierarchy));
		EXPECT_FAIL(model_result);
	}

	SUBCASE("Root node with OOB mesh index")
	{
		// Root node has OOB mesh index
		const std::vector nodes = {
			model::ParentOnlyNode{.parent_index = {}, .data = {.mesh_index = 100}}
		};
		auto hierarchy_result = model::Hierarchy::create(nodes);
		EXPECT_SUCCESS(hierarchy_result);
		auto hierarchy = std::move(*hierarchy_result);

		auto model_result =
			model::Model::assemble(std::move(material_list), {valid_mesh}, std::move(hierarchy));
		EXPECT_FAIL(model_result);
	}
}

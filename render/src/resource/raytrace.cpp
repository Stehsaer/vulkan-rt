#include "render/resource/raytrace.hpp"
#include "common/util/error.hpp"
#include "render/model/mesh.hpp"
#include "render/model/model.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/util/trivial-descriptor-set.hpp"

#include <expected>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	std::expected<RaytraceResourceLayout, Error> RaytraceResourceLayout::create(
		const vulkan::Context& context
	) noexcept
	{
		auto layout_result = vulkan::trivset::Layout<MeshInput>::create(context);
		if (!layout_result) return layout_result.error().forward("Create layout failed");
		return RaytraceResourceLayout(std::move(*layout_result));
	}

	std::expected<RaytraceResource, Error> RaytraceResource::create(
		const vulkan::Context& context,
		const RaytraceResourceLayout& layout,
		const Model& model
	) noexcept
	{
		/*===== Create descriptor set =====*/

		auto sets_result = layout.get_layout().create_sets(context, 1);
		if (!sets_result) return sets_result.error().forward("Create descriptor sets failed");
		auto set = std::move((*sets_result)[0]);

		/*===== Write descriptor set =====*/

		using namespace vulkan::trivset;

		const auto input = RaytraceResourceLayout::MeshInput{
			.primitive_attr = slot::StorageBuffer(model.mesh_list->primitive_attr_buffer, 0, vk::WholeSize),
			.vertex_buffer = slot::StorageBuffer(model.mesh_list->vertex_buffer, 0, vk::WholeSize),
			.index_buffer = slot::StorageBuffer(model.mesh_list->index_buffer, 0, vk::WholeSize),
		};

		set.update(context, input);

		return RaytraceResource(std::move(set));
	}
}

#include "render/model/blas.hpp"
#include "common/util/error.hpp"
#include "model/blas.hpp"
#include "render/model/material.hpp"
#include "render/model/mesh.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <functional>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	std::expected<BlasList, Error> BlasList::create(
		const vulkan::Context& context,
		const MeshList& mesh_list,
		const MaterialList& material_list
	) noexcept
	{
		if (!context.feature.raytracing)
			return Error("Missing raytracing feature", "BLAS requires raytracing feature to be enabled");

		return impl::create_blas(context, mesh_list, material_list)
			.and_then(std::bind(impl::build_blas, std::cref(context), std::placeholders::_1))
			.transform([](impl::BuildBlasResult result) { return BlasList(std::move(result.blas_list)); });
	}
}

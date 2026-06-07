#include "resource/pipeline.hpp"
#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "render/model/material.hpp"
#include "render/model/model.hpp"
#include "render/pipeline/auto-exposure.hpp"
#include "render/pipeline/composite.hpp"
#include "render/pipeline/deferred.hpp"
#include "render/pipeline/direct.hpp"
#include "render/pipeline/indirect.hpp"
#include "resource/aux-resource.hpp"
#include "resource/render-resource.hpp"
#include "vulkan/interface/context.hpp"

#include <cstdint>
#include <expected>
#include <libassert/assert.hpp>
#include <ranges>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace resource
{
	std::expected<Pipeline, Error> Pipeline::create(
		const vulkan::Context& context,
		const render::MaterialLayout& material_layout,
		vk::Format composite_format
	) noexcept
	{
		auto indirect_pipeline_result = render::IndirectPipeline::create(context);
		if (!indirect_pipeline_result)
			return indirect_pipeline_result.error().forward("Create indirect pipeline failed");
		auto indirect_pipeline = std::move(*indirect_pipeline_result);

		auto deferred_pipeline_result = render::DeferredPipeline::create(context, material_layout);
		if (!deferred_pipeline_result)
			return deferred_pipeline_result.error().forward("Create deferred pipeline failed");
		auto deferred_pipeline = std::move(*deferred_pipeline_result);

		auto direct_lighting_pipeline_result = render::DirectLightingPipeline::create(context);
		if (!direct_lighting_pipeline_result)
			return direct_lighting_pipeline_result.error().forward("Create direct lighting pipeline failed");
		auto direct_lighting_pipeline = std::move(*direct_lighting_pipeline_result);

		auto auto_exposure_pipeline_result = render::AutoExposurePipeline::create(context);
		if (!auto_exposure_pipeline_result)
			return auto_exposure_pipeline_result.error().forward("Create auto-exposure pipeline failed");
		auto auto_exposure_pipeline = std::move(*auto_exposure_pipeline_result);

		auto composite_pipeline_result = render::CompositePipeline::create(context, composite_format);
		if (!composite_pipeline_result)
			return composite_pipeline_result.error().forward("Create composite pipeline failed");
		auto composite_pipeline = std::move(*composite_pipeline_result);

		return Pipeline{
			.indirect = std::move(indirect_pipeline),
			.deferred = std::move(deferred_pipeline),
			.direct_lighting = std::move(direct_lighting_pipeline),
			.auto_exposure = std::move(auto_exposure_pipeline),
			.composite = std::move(composite_pipeline)
		};
	}

	std::expected<std::vector<ResourceSet>, Error> Pipeline::create_resource_sets(
		const vulkan::Context& context,
		uint32_t count
	) const noexcept
	{
		auto indirect_resource_set_result = indirect.create_resource_sets(context, count);
		if (!indirect_resource_set_result)
			return indirect_resource_set_result.error().forward(
				"Create resource sets for indirect pipeline failed"
			);
		auto indirect_resource_sets = std::move(*indirect_resource_set_result);

		auto deferred_resource_set_result = deferred.create_resource_sets(context, count);
		if (!deferred_resource_set_result)
			return deferred_resource_set_result.error().forward(
				"Create resource sets for deferred pipeline failed"
			);
		auto deferred_resource_sets = std::move(*deferred_resource_set_result);

		auto direct_lighting_resource_set_result = direct_lighting.create_resource_sets(context, count);
		if (!direct_lighting_resource_set_result)
			return direct_lighting_resource_set_result.error().forward(
				"Create resource sets for direct lighting pipeline failed"
			);
		auto direct_lighting_resource_sets = std::move(*direct_lighting_resource_set_result);

		auto auto_exposure_resource_set_result = auto_exposure.create_resource_sets(context, count);
		if (!auto_exposure_resource_set_result)
			return auto_exposure_resource_set_result.error().forward(
				"Create resource sets for auto-exposure pipeline failed"
			);
		auto auto_exposure_resource_sets = std::move(*auto_exposure_resource_set_result);

		auto composite_resource_set_result = composite.create_resource_sets(context, count);
		if (!composite_resource_set_result)
			return composite_resource_set_result.error().forward(
				"Create resource sets for composite pipeline failed"
			);
		auto composite_resource_sets = std::move(*composite_resource_set_result);

		return std::views::zip_transform(
				   CTOR_LAMBDA(ResourceSet),
				   indirect_resource_sets | std::views::as_rvalue,
				   deferred_resource_sets | std::views::as_rvalue,
				   direct_lighting_resource_sets | std::views::as_rvalue,
				   auto_exposure_resource_sets | std::views::as_rvalue,
				   composite_resource_sets | std::views::as_rvalue
			   )
			| std::ranges::to<std::vector>();
	}

	void ResourceSet::update(
		const vulkan::Context& context,
		const render::Model& model,
		const resource::RenderResource& curr_resource,
		const resource::RenderResource& prev_resource,
		const resource::AuxResource& aux_resource
	) noexcept
	{
		DEBUG_ASSERT(curr_resource.attachments.has_value());
		DEBUG_ASSERT(prev_resource.attachments.has_value());

		indirect.update(context, model, curr_resource.drawcall, curr_resource.indirect);

		deferred.update(
			context,
			model,
			curr_resource.drawcall,
			curr_resource.indirect,
			curr_resource.attachments->deferred,
			curr_resource.attachments->hdr,
			curr_resource.param->camera,
			curr_resource.param->primary_light
		);

		direct_lighting.update(
			context,
			curr_resource.attachments->deferred,
			curr_resource.attachments->hdr,
			curr_resource.param->camera,
			curr_resource.param->primary_light
		);

		auto_exposure.update(
			context,
			curr_resource.auto_exposure,
			prev_resource.auto_exposure,
			curr_resource.param->exposure_param,
			curr_resource.attachments->hdr,
			aux_resource.exposure_mask_view
		);

		composite.update(
			context,
			curr_resource.auto_exposure->exposure_result_buffer,
			curr_resource.attachments->hdr
		);
	}
}

#include "resource/pipeline.hpp"
#include "common/util/construct.hpp"
#include "common/util/error.hpp"
#include "render/model/material.hpp"
#include "render/model/model.hpp"
#include "render/model/tlas.hpp"
#include "render/pipeline/auto-exposure.hpp"
#include "render/pipeline/composite.hpp"
#include "render/pipeline/deferred.hpp"
#include "render/pipeline/direct.hpp"
#include "render/pipeline/downsample.hpp"
#include "render/pipeline/indirect.hpp"
#include "render/pipeline/motion-vector.hpp"
#include "render/pipeline/shadow/spatial-denoise.hpp"
#include "render/pipeline/shadow/spatial-variance.hpp"
#include "render/pipeline/shadow/temporal-denoise.hpp"
#include "render/pipeline/shadow/trace.hpp"
#include "render/resource/raytrace.hpp"
#include "resource/aux-resource.hpp"
#include "resource/render-resource.hpp"
#include "vulkan/interface/context.hpp"

#include <cstdint>
#include <expected>
#include <glm/ext/vector_int2_sized.hpp>
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
		const render::RaytraceResourceLayout& raytrace_resource_layout,
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

		auto downsample_pipeline_result = render::DownsamplePipeline::create(context);
		if (!downsample_pipeline_result)
			return downsample_pipeline_result.error().forward("Create downsample pipeline failed");
		auto downsample_pipeline = std::move(*downsample_pipeline_result);

		auto motion_vector_pipeline_result = render::MotionVectorPipeline::create(context);
		if (!motion_vector_pipeline_result)
			return motion_vector_pipeline_result.error().forward("Create motion vector pipeline failed");
		auto motion_vector_pipeline = std::move(*motion_vector_pipeline_result);

		auto shadow_trace_pipeline_result =
			render::shadow::RaytracePipeline::create(context, material_layout, raytrace_resource_layout);
		if (!shadow_trace_pipeline_result)
			return shadow_trace_pipeline_result.error().forward("Create shadow tracing pipeline failed");
		auto shadow_trace_pipeline = std::move(*shadow_trace_pipeline_result);

		auto shadow_spatial_variance_pipeline_result =
			render::shadow::SpatialVariancePipeline::create(context);
		if (!shadow_spatial_variance_pipeline_result)
			return shadow_spatial_variance_pipeline_result.error().forward(
				"Create shadow spatial variance pipeline failed"
			);
		auto shadow_spatial_variance_pipeline = std::move(*shadow_spatial_variance_pipeline_result);

		auto shadow_temporal_denoise_pipeline_result =
			render::shadow::TemporalDenoisePipeline::create(context);
		if (!shadow_temporal_denoise_pipeline_result)
			return shadow_temporal_denoise_pipeline_result.error().forward(
				"Create shadow temporal denoise pipeline failed"
			);
		auto shadow_temporal_denoise_pipeline = std::move(*shadow_temporal_denoise_pipeline_result);

		auto shadow_spatial_denoise_pipeline_result = render::shadow::SpatialDenoisePipeline::create(context);
		if (!shadow_spatial_denoise_pipeline_result)
			return shadow_spatial_denoise_pipeline_result.error().forward(
				"Create shadow denoise pipeline failed"
			);
		auto shadow_spatial_denoise_pipeline = std::move(*shadow_spatial_denoise_pipeline_result);

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
			.downsample = std::move(downsample_pipeline),
			.motion_vector = std::move(motion_vector_pipeline),
			.shadow_trace = std::move(shadow_trace_pipeline),
			.shadow_spatial_variance = std::move(shadow_spatial_variance_pipeline),
			.shadow_temporal_denoise = std::move(shadow_temporal_denoise_pipeline),
			.shadow_spatial_denoise = std::move(shadow_spatial_denoise_pipeline),
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

		auto downsample_resource_set_result = downsample.create_resource_sets(context, count);
		if (!downsample_resource_set_result)
			return downsample_resource_set_result.error().forward(
				"Create resource sets for downsample pipeline failed"
			);
		auto downsample_resource_sets = std::move(*downsample_resource_set_result);

		auto motion_vector_resource_set_result = motion_vector.create_resource_sets(context, count);
		if (!motion_vector_resource_set_result)
			return motion_vector_resource_set_result.error().forward(
				"Create resource sets for motion vector pipeline failed"
			);
		auto motion_vector_resource_sets = std::move(*motion_vector_resource_set_result);

		auto shadow_trace_resource_set_result = shadow_trace.create_resource_sets(context, count);
		if (!shadow_trace_resource_set_result)
			return shadow_trace_resource_set_result.error().forward(
				"Create resource sets for shadow raytracing pipeline failed"
			);
		auto shadow_trace_resource_sets = std::move(*shadow_trace_resource_set_result);

		auto shadow_spatial_variance_resource_set_result =
			shadow_spatial_variance.create_resource_sets(context, count);
		if (!shadow_spatial_variance_resource_set_result)
			return shadow_spatial_variance_resource_set_result.error().forward(
				"Create resource sets for shadow denoise pipeline failed"
			);
		auto shadow_spatial_variance_resource_sets = std::move(*shadow_spatial_variance_resource_set_result);

		auto shadow_temporal_denoise_resource_set_result =
			shadow_temporal_denoise.create_resource_sets(context, count);
		if (!shadow_temporal_denoise_resource_set_result)
			return shadow_temporal_denoise_resource_set_result.error().forward(
				"Create resource sets for shadow temporal denoise pipeline failed"
			);
		auto shadow_temporal_denoise_resource_sets = std::move(*shadow_temporal_denoise_resource_set_result);

		auto shadow_spatial_denoise_resource_set_result =
			shadow_spatial_denoise.create_resource_sets(context, count);
		if (!shadow_spatial_denoise_resource_set_result)
			return shadow_spatial_denoise_resource_set_result.error().forward(
				"Create resource sets for shadow denoise pipeline failed"
			);
		auto shadow_spatial_denoise_resource_sets = std::move(*shadow_spatial_denoise_resource_set_result);

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
				   downsample_resource_sets | std::views::as_rvalue,
				   motion_vector_resource_sets | std::views::as_rvalue,
				   shadow_trace_resource_sets | std::views::as_rvalue,
				   shadow_spatial_variance_resource_sets | std::views::as_rvalue,
				   shadow_temporal_denoise_resource_sets | std::views::as_rvalue,
				   shadow_spatial_denoise_resource_sets | std::views::as_rvalue,
				   direct_lighting_resource_sets | std::views::as_rvalue,
				   auto_exposure_resource_sets | std::views::as_rvalue,
				   composite_resource_sets | std::views::as_rvalue
			   )
			| std::ranges::to<std::vector>();
	}

	void ResourceSet::update(
		const vulkan::Context& context,
		const render::Model& model,
		const render::Tlas& tlas,
		const render::RaytraceResource& raytrace_res,
		const resource::RenderResource& curr_resource,
		const resource::RenderResource& prev_resource,
		const resource::AuxResource& aux_resource,
		uint32_t frame_index
	) noexcept
	{
		DEBUG_ASSERT(curr_resource.attachments.has_value());
		DEBUG_ASSERT(prev_resource.attachments.has_value());

		indirect.update(
			context,
			model,
			curr_resource.param->camera,
			curr_resource.drawcall,
			curr_resource.indirect
		);

		deferred.update(
			context,
			model,
			curr_resource.drawcall,
			curr_resource.indirect,
			curr_resource.attachments->deferred,
			curr_resource.attachments->hdr,
			curr_resource.param->camera
		);

		downsample
			.update(context, curr_resource.attachments->deferred, curr_resource.attachments->half_deferred);

		motion_vector.update(
			context,
			curr_resource.attachments->motion_vector,
			curr_resource.attachments->half_deferred,
			prev_resource.attachments->half_deferred,
			curr_resource.param->camera
		);

		shadow_trace.update(
			context,
			model.material_list,
			tlas,
			raytrace_res,
			curr_resource.attachments->half_deferred,
			curr_resource.attachments->shadow,
			curr_resource.param->camera,
			curr_resource.param->primary_light,
			aux_resource.stbn_noise.view,
			frame_index
		);

		shadow_spatial_variance.update(
			context,
			curr_resource.attachments->half_deferred,
			curr_resource.attachments->shadow,
			curr_resource.param->camera
		);

		shadow_temporal_denoise.update(
			context,
			curr_resource.attachments->shadow,
			prev_resource.attachments->shadow,
			curr_resource.attachments->motion_vector
		);

		shadow_spatial_denoise.update(
			context,
			curr_resource.param->camera,
			curr_resource.attachments->half_deferred,
			curr_resource.attachments->shadow
		);

		direct_lighting.update(
			context,
			curr_resource.attachments->deferred,
			curr_resource.attachments->hdr,
			curr_resource.attachments->shadow,
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

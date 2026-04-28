#include "resource/pipeline.hpp"
#include "common/util/construct.hpp"
#include "render/resource/forward-rendering.hpp"

namespace resource
{
	std::expected<Pipeline, Error> Pipeline::create(
		const vulkan::DeviceContext& context,
		const render::MaterialLayout& material_layout,
		vk::Format composite_format
	) noexcept
	{
		auto indirect_pipeline_result = render::IndirectPipeline::create(context);
		if (!indirect_pipeline_result)
			return indirect_pipeline_result.error().forward("Create indirect pipeline failed");
		auto indirect_pipeline = std::move(*indirect_pipeline_result);

		auto forward_pipeline_result = render::ForwardPipeline::create(
			context,
			material_layout,
			render::ForwardRenderResource::HDR_FORMAT,
			render::ForwardRenderResource::DEPTH_FORMAT
		);
		if (!forward_pipeline_result)
			return forward_pipeline_result.error().forward("Create graphic pipeline failed");
		auto forward_pipeline = std::move(*forward_pipeline_result);

		auto auto_exposure_pipeline_result = render::AutoExposurePipeline::create(context);
		if (!auto_exposure_pipeline_result)
			return auto_exposure_pipeline_result.error().forward("Create auto-exposure pipeline failed");
		auto auto_exposure_pipeline = std::move(*auto_exposure_pipeline_result);

		auto composite_pipeline_result = render::CompositePipeline::create(context, composite_format);
		if (!composite_pipeline_result)
			return composite_pipeline_result.error().forward("Create composite pipeline failed");
		auto composite_pipeline = std::move(*composite_pipeline_result);

		return Pipeline{
			.indirect_pipeline = std::move(indirect_pipeline),
			.forward_pipeline = std::move(forward_pipeline),
			.auto_exposure_pipeline = std::move(auto_exposure_pipeline),
			.composite_pipeline = std::move(composite_pipeline)
		};
	}

	std::expected<std::vector<ResourceSet>, Error> Pipeline::create_resource_sets(
		const vulkan::DeviceContext& context,
		uint32_t count
	) const noexcept
	{
		auto indirect_resource_set_result = indirect_pipeline.create_resource_sets(context, count);
		if (!indirect_resource_set_result)
			return indirect_resource_set_result.error().forward(
				"Create resource sets for indirect pipeline failed"
			);
		auto indirect_resource_sets = std::move(*indirect_resource_set_result);

		auto forward_resource_set_result = forward_pipeline.create_resource_sets(context, count);
		if (!forward_resource_set_result)
			return forward_resource_set_result.error().forward(
				"Create resource sets for forward pipeline failed"
			);
		auto forward_resource_sets = std::move(*forward_resource_set_result);

		auto auto_exposure_resource_set_result = auto_exposure_pipeline.create_resource_sets(context, count);
		if (!auto_exposure_resource_set_result)
			return auto_exposure_resource_set_result.error().forward(
				"Create resource sets for auto-exposure pipeline failed"
			);
		auto auto_exposure_resource_sets = std::move(*auto_exposure_resource_set_result);

		auto composite_resource_set_result = composite_pipeline.create_resource_sets(context, count);
		if (!composite_resource_set_result)
			return composite_resource_set_result.error().forward(
				"Create resource sets for composite pipeline failed"
			);
		auto composite_resource_sets = std::move(*composite_resource_set_result);

		return std::views::zip_transform(
				   CTOR_LAMBDA(ResourceSet),
				   indirect_resource_sets | std::views::as_rvalue,
				   forward_resource_sets | std::views::as_rvalue,
				   auto_exposure_resource_sets | std::views::as_rvalue,
				   composite_resource_sets | std::views::as_rvalue
			   )
			| std::ranges::to<std::vector>();
	}

	void ResourceSet::update(
		const vulkan::DeviceContext& context,
		const render::Model& model,
		const resource::RenderResource& curr_resource,
		const resource::RenderResource& prev_resource,
		const resource::AuxResource& aux_resource,
		const vulkan::SwapchainContext::Frame& swapchain_frame
	) noexcept
	{
		indirect_resource_set.update(context, model, curr_resource.drawcall, curr_resource.indirect);

		forward_resource_set.update(
			context,
			model,
			curr_resource.param->camera,
			curr_resource.param->primary_light,
			curr_resource.drawcall,
			curr_resource.indirect,
			curr_resource.forward,
			swapchain_frame.extent
		);

		auto_exposure_resource_set.update(
			context,
			curr_resource.param->exposure_param,
			curr_resource.auto_exposure,
			prev_resource.auto_exposure,
			aux_resource.exposure_mask_view,
			curr_resource.forward->hdr.view,
			swapchain_frame.extent
		);

		composite_resource_set
			.update(context, curr_resource.auto_exposure, curr_resource.forward, swapchain_frame.extent);
	}
}

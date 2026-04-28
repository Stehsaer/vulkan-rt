#pragma once

#include "common/util/error.hpp"
#include "render/model/material.hpp"
#include "render/pipeline/auto-exposure.hpp"
#include "render/pipeline/composite.hpp"
#include "render/pipeline/forward.hpp"
#include "render/pipeline/indirect.hpp"

#include "aux-resource.hpp"
#include "render-resource.hpp"
#include "vulkan/context/swapchain.hpp"

namespace resource
{
	struct ResourceSet;

	///
	/// @brief All pipelines
	///
	struct Pipeline
	{
		render::IndirectPipeline indirect_pipeline;
		render::ForwardPipeline forward_pipeline;
		render::AutoExposurePipeline auto_exposure_pipeline;
		render::CompositePipeline composite_pipeline;

		///
		/// @brief Create pipelines
		///
		/// @param context Vulkan context
		/// @param material_layout Material layout from model
		/// @param composite_format Format of output attachment
		/// @return Created pipelines or error
		///
		[[nodiscard]]
		static std::expected<Pipeline, Error> create(
			const vulkan::DeviceContext& context,
			const render::MaterialLayout& material_layout,
			vk::Format composite_format
		) noexcept;

		///
		/// @brief Create given numbers of resource sets
		///
		/// @param context Vulkan context
		/// @param count Number of resource sets
		/// @return Created resource sets or error
		///
		[[nodiscard]]
		std::expected<std::vector<ResourceSet>, Error> create_resource_sets(
			const vulkan::DeviceContext& context,
			uint32_t count
		) const noexcept;
	};

	///
	/// @brief Resource sets of the pipelines
	///
	struct ResourceSet
	{
		render::IndirectPipeline::ResourceSet indirect_resource_set;
		render::ForwardPipeline::ResourceSet forward_resource_set;
		render::AutoExposurePipeline::ResourceSet auto_exposure_resource_set;
		render::CompositePipeline::ResourceSet composite_resource_set;

		///
		/// @brief Update the resource sets
		///
		/// @param context Vulkan context
		/// @param model Model instance
		/// @param curr_resource Render resource of current frame
		/// @param prev_resource Render resource of previous frame
		/// @param aux_resource Auxiliary resource
		/// @param swapchain_frame Swapchain frame of current frame
		///
		void update(
			const vulkan::DeviceContext& context,
			const render::Model& model,
			const resource::RenderResource& curr_resource,
			const resource::RenderResource& prev_resource,
			const resource::AuxResource& aux_resource,
			const vulkan::SwapchainContext::Frame& swapchain_frame
		) noexcept;
	};
}

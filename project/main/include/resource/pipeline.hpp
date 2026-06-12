#pragma once

#include "aux-resource.hpp"
#include "common/util/error.hpp"
#include "render-resource.hpp"
#include "render/model/material.hpp"
#include "render/model/model.hpp"
#include "render/model/tlas.hpp"
#include "render/pipeline/auto-exposure.hpp"
#include "render/pipeline/composite.hpp"
#include "render/pipeline/deferred.hpp"
#include "render/pipeline/direct.hpp"
#include "render/pipeline/downsample.hpp"
#include "render/pipeline/indirect.hpp"
#include "render/pipeline/shadow/raytrace.hpp"
#include "render/resource/raytrace.hpp"
#include "vulkan/interface/context.hpp"

#include <cstdint>
#include <expected>
#include <glm/ext/vector_int2_sized.hpp>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace resource
{
	struct ResourceSet;

	///
	/// @brief All pipelines
	///
	struct Pipeline
	{
		render::IndirectPipeline indirect;
		render::DeferredPipeline deferred;
		render::DownsamplePipeline downsample;
		render::shadow::RaytracePipeline shadow_trace;
		render::DirectLightingPipeline direct_lighting;
		render::AutoExposurePipeline auto_exposure;
		render::CompositePipeline composite;

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
			const vulkan::Context& context,
			const render::MaterialLayout& material_layout,
			const render::RaytraceResourceLayout& raytrace_resource_layout,
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
			const vulkan::Context& context,
			uint32_t count
		) const noexcept;
	};

	///
	/// @brief Resource sets of the pipelines
	///
	struct ResourceSet
	{
		render::IndirectPipeline::ResourceSet indirect;
		render::DeferredPipeline::ResourceSet deferred;
		render::DownsamplePipeline::ResourceSet downsample;
		render::shadow::RaytracePipeline::ResourceSet shadow_trace;
		render::DirectLightingPipeline::ResourceSet direct_lighting;
		render::AutoExposurePipeline::ResourceSet auto_exposure;
		render::CompositePipeline::ResourceSet composite;

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
			const vulkan::Context& context,
			const render::Model& model,
			const render::Tlas& tlas,
			const render::RaytraceResource& raytrace_res,
			const resource::RenderResource& curr_resource,
			const resource::RenderResource& prev_resource,
			const resource::AuxResource& aux_resource,
			glm::i32vec2 noise_offset
		) noexcept;
	};
}

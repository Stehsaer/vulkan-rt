#pragma once

#include "common/util/error.hpp"
#include "render/resource/auto-exposure.hpp"
#include "vulkan/interface/common-context.hpp"

#include <vulkan/vulkan_raii.hpp>

namespace render
{
	///
	/// @brief Auto exposure pipeline
	/// @details
	/// - Takes the HDR image as input, calculates the histogram and compute the exposure value
	/// - Configure by setting the exposure parameters (`ExposureParam`)
	///
	class AutoExposurePipeline
	{
	  public:

		class ResourceSet;

		///
		/// @brief Create a auto-exposure pipeline
		///
		/// @param context Vulkan context
		/// @return Created pipeline or error
		///
		[[nodiscard]]
		static std::expected<AutoExposurePipeline, Error> create(
			const vulkan::DeviceContext& context
		) noexcept;

		///
		/// @brief Create a given number of resource sets
		///
		/// @param context Vulkan context
		/// @param count Number of resource sets to create
		/// @return Created resource sets or error
		///
		[[nodiscard]]
		std::expected<std::vector<ResourceSet>, Error> create_resource_sets(
			const vulkan::DeviceContext& context,
			uint32_t count
		) const noexcept;

		///
		/// @brief Execute computes for auto-exposure
		///
		/// @param command_buffer Command buffer to record commands
		/// @param resource_set Resource set to bind for the compute
		///
		void compute(
			const vk::raii::CommandBuffer& command_buffer,
			const ResourceSet& resource_set
		) const noexcept;

	  private:

		vk::raii::DescriptorSetLayout clear_resource_layout, histogram_resource_layout,
			reduce_resource_layout;
		vk::raii::PipelineLayout clear_pipeline_layout, histogram_pipeline_layout, reduce_pipeline_layout;
		vk::raii::Pipeline clear_pipeline, histogram_pipeline, reduce_pipeline;

		vk::raii::Sampler input_sampler;
		vk::raii::Sampler mask_sampler;

		explicit AutoExposurePipeline(
			vk::raii::DescriptorSetLayout clear_resource_layout,
			vk::raii::DescriptorSetLayout histogram_resource_layout,
			vk::raii::DescriptorSetLayout reduce_resource_layout,
			vk::raii::PipelineLayout clear_pipeline_layout,
			vk::raii::PipelineLayout histogram_pipeline_layout,
			vk::raii::PipelineLayout reduce_pipeline_layout,
			vk::raii::Pipeline clear_pipeline,
			vk::raii::Pipeline histogram_pipeline,
			vk::raii::Pipeline reduce_pipeline,
			vk::raii::Sampler input_sampler,
			vk::raii::Sampler mask_sampler
		) :
			clear_resource_layout(std::move(clear_resource_layout)),
			histogram_resource_layout(std::move(histogram_resource_layout)),
			reduce_resource_layout(std::move(reduce_resource_layout)),
			clear_pipeline_layout(std::move(clear_pipeline_layout)),
			histogram_pipeline_layout(std::move(histogram_pipeline_layout)),
			reduce_pipeline_layout(std::move(reduce_pipeline_layout)),
			clear_pipeline(std::move(clear_pipeline)),
			histogram_pipeline(std::move(histogram_pipeline)),
			reduce_pipeline(std::move(reduce_pipeline)),
			input_sampler(std::move(input_sampler)),
			mask_sampler(std::move(mask_sampler))
		{}

	  public:

		AutoExposurePipeline(const AutoExposurePipeline&) = delete;
		AutoExposurePipeline(AutoExposurePipeline&&) = default;
		AutoExposurePipeline& operator=(const AutoExposurePipeline&) = delete;
		AutoExposurePipeline& operator=(AutoExposurePipeline&&) = default;
	};

	///
	/// @brief Resource set for auto-exposure pipeline
	///
	class AutoExposurePipeline::ResourceSet
	{
	  public:

		///
		/// @brief Update the resource set
		///
		/// @param context Vulkan context
		/// @param exposure_param Exposure parameters for current frame
		/// @param resource Auto-exposure resources of current frame
		/// @param prev_resource Auto-exposure resources of previous frame
		/// @param mask_image_view Image view of the mask image
		/// @param input_image_view Input image view
		/// @param image_size Image size
		///
		void update(
			const vulkan::DeviceContext& context,
			const vulkan::ElementBufferRef<ExposureParam>& exposure_param,
			const AutoExposureResource& resource,
			const AutoExposureResource& prev_resource,
			vk::ImageView mask_image_view,
			vk::ImageView input_image_view,
			glm::u32vec2 image_size
		) noexcept;

	  private:

		std::shared_ptr<vk::raii::DescriptorPool> descriptor_pool;
		vk::raii::DescriptorSet clear_descriptor_set, histogram_descriptor_set, reduce_descriptor_set;

		vk::Sampler input_sampler;
		vk::Sampler mask_sampler;

		glm::u32vec2 image_size;

		explicit ResourceSet(
			std::shared_ptr<vk::raii::DescriptorPool> descriptor_pool,
			vk::raii::DescriptorSet clear_descriptor_set,
			vk::raii::DescriptorSet histogram_descriptor_set,
			vk::raii::DescriptorSet reduce_descriptor_set,
			vk::Sampler input_sampler,
			vk::Sampler mask_sampler
		) :
			descriptor_pool(std::move(descriptor_pool)),
			clear_descriptor_set(std::move(clear_descriptor_set)),
			histogram_descriptor_set(std::move(histogram_descriptor_set)),
			reduce_descriptor_set(std::move(reduce_descriptor_set)),
			input_sampler(input_sampler),
			mask_sampler(mask_sampler)
		{}

		friend class AutoExposurePipeline;

	  public:

		ResourceSet(const ResourceSet&) = delete;
		ResourceSet(ResourceSet&&) = default;
		ResourceSet& operator=(const ResourceSet&) = delete;
		ResourceSet& operator=(ResourceSet&&) = default;
	};
}

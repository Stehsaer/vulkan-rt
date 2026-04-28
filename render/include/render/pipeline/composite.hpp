#pragma once

#include "render/resource/auto-exposure.hpp"
#include "render/resource/forward-rendering.hpp"
#include "vulkan/interface/common-context.hpp"

#include <glm/glm.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	///
	/// @brief Composite pipeline
	/// @details Takes the HDR image and exposure result, composites and tonemaps the image
	///
	class CompositePipeline
	{
	  public:

		class ResourceSet;

		///
		/// @brief Create a composite pipeline
		///
		/// @param context Vulkan context
		/// @param target_format Format of the target image
		/// @return Created pipeline or error
		///
		[[nodiscard]]
		static std::expected<CompositePipeline, Error> create(
			const vulkan::DeviceContext& context,
			vk::Format target_format
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
		/// @brief Execute the composite pass
		///
		/// @param command_buffer Command buffer
		/// @param resource_binding Resource set
		///
		void render(
			const vk::raii::CommandBuffer& command_buffer,
			const ResourceSet& resource_binding
		) const noexcept;

	  private:

		vk::raii::DescriptorSetLayout resource_layout;
		vk::raii::PipelineLayout pipeline_layout;
		vk::raii::Pipeline pipeline;
		vk::raii::Sampler input_sampler;

		explicit CompositePipeline(
			vk::raii::DescriptorSetLayout resource_layout,
			vk::raii::PipelineLayout pipeline_layout,
			vk::raii::Pipeline pipeline,
			vk::raii::Sampler input_sampler
		) :
			resource_layout(std::move(resource_layout)),
			pipeline_layout(std::move(pipeline_layout)),
			pipeline(std::move(pipeline)),
			input_sampler(std::move(input_sampler))
		{}

	  public:

		CompositePipeline(const CompositePipeline&) = delete;
		CompositePipeline(CompositePipeline&&) = default;
		CompositePipeline& operator=(const CompositePipeline&) = delete;
		CompositePipeline& operator=(CompositePipeline&&) = default;
	};

	///
	/// @brief Resource set for composite pipeline
	///
	class CompositePipeline::ResourceSet
	{
	  public:

		///
		/// @brief Update the resource set
		///
		/// @param context Vulkan context
		/// @param exposure_resource Exposure resource of current frame
		/// @param forward_resource Forward rendering resource of current frame
		/// @param image_size Size of the input and output image
		///
		void update(
			const vulkan::DeviceContext& context,
			const AutoExposureResource& exposure_resource,
			const ForwardRenderResource& forward_resource,
			glm::u32vec2 image_size
		) noexcept;

	  private:

		std::shared_ptr<vk::raii::DescriptorPool> descriptor_pool;
		vk::raii::DescriptorSet descriptor_set;

		vk::Sampler sampler;

		glm::u32vec2 image_size = {0, 0};

		explicit ResourceSet(
			std::shared_ptr<vk::raii::DescriptorPool> descriptor_pool,
			vk::raii::DescriptorSet descriptor_set,
			vk::Sampler sampler
		) :
			descriptor_pool(std::move(descriptor_pool)),
			descriptor_set(std::move(descriptor_set)),
			sampler(sampler)
		{}

		friend class CompositePipeline;

	  public:

		ResourceSet(const ResourceSet&) = delete;
		ResourceSet(ResourceSet&&) = default;
		ResourceSet& operator=(const ResourceSet&) = delete;
		ResourceSet& operator=(ResourceSet&&) = default;
	};
}

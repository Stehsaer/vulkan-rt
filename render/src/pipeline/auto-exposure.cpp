#include "render/pipeline/auto-exposure.hpp"
#include "common/util/array.hpp"
#include "common/util/construct.hpp"
#include "shader/auto-exposure/clear.hpp"
#include "shader/auto-exposure/histogram.hpp"
#include "shader/auto-exposure/reduce.hpp"
#include "vulkan/util/pool-size.hpp"
#include "vulkan/util/shader.hpp"

#include <glm/glm.hpp>
#include <ranges>
#include <vulkan/vulkan_structs.hpp>

namespace render
{
	namespace
	{
		consteval auto get_clear_program_resource_bindings() noexcept
		{
			constexpr auto histogram_binding = vk::DescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute
			};

			return std::to_array({histogram_binding});
		}

		consteval auto get_histogram_program_resource_bindings() noexcept
		{
			constexpr auto param_binding = vk::DescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute
			};

			constexpr auto input_image_binding = vk::DescriptorSetLayoutBinding{
				.binding = 1,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute
			};

			constexpr auto histogram_binding = vk::DescriptorSetLayoutBinding{
				.binding = 2,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute
			};

			constexpr auto mask_image_binding = vk::DescriptorSetLayoutBinding{
				.binding = 3,
				.descriptorType = vk::DescriptorType::eCombinedImageSampler,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute
			};

			return std::to_array({
				param_binding,
				input_image_binding,
				histogram_binding,
				mask_image_binding,
			});
		}

		consteval auto get_reduce_program_resource_bindings() noexcept
		{
			constexpr auto param_binding = vk::DescriptorSetLayoutBinding{
				.binding = 0,
				.descriptorType = vk::DescriptorType::eUniformBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute
			};

			constexpr auto histogram_binding = vk::DescriptorSetLayoutBinding{
				.binding = 1,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute
			};

			constexpr auto exposure_result_binding = vk::DescriptorSetLayoutBinding{
				.binding = 2,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute
			};

			constexpr auto last_frame_binding = vk::DescriptorSetLayoutBinding{
				.binding = 3,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute
			};

			constexpr auto current_frame_binding = vk::DescriptorSetLayoutBinding{
				.binding = 4,
				.descriptorType = vk::DescriptorType::eStorageBuffer,
				.descriptorCount = 1,
				.stageFlags = vk::ShaderStageFlagBits::eCompute
			};

			return std::to_array({
				param_binding,
				histogram_binding,
				exposure_result_binding,
				last_frame_binding,
				current_frame_binding,
			});
		}

		constexpr glm::uvec3 HISTOGRAM_WORKGROUP_SIZE = {16, 16, 1};

		constexpr auto CLEAR_RESOURCE_BINDINGS = get_clear_program_resource_bindings();
		constexpr auto HISTOGRAM_RESOURCE_BINDINGS = get_histogram_program_resource_bindings();
		constexpr auto REDUCE_RESOURCE_BINDINGS = get_reduce_program_resource_bindings();
	}

	void AutoExposurePipeline::compute(
		const vk::raii::CommandBuffer& command_buffer,
		const ResourceSet& resource_set
	) const noexcept
	{
		/*===== Clear Histogram =====*/

		command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, clear_pipeline);
		command_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eCompute,
			clear_pipeline_layout,
			0,
			{resource_set.clear_descriptor_set},
			{}
		);
		command_buffer.dispatch(1, 1, 1);

		/*===== Post-Clear Sync =====*/

		const auto clear_barrier = vk::MemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
			.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
			.dstAccessMask = vk::AccessFlagBits2::eShaderRead | vk::AccessFlagBits2::eShaderWrite
		};
		command_buffer.pipelineBarrier2(vk::DependencyInfo().setMemoryBarriers(clear_barrier));

		/*===== Histogram =====*/

		const auto histogram_group_count =
			(glm::u32vec3(resource_set.image_size, 1) + HISTOGRAM_WORKGROUP_SIZE - uint32_t(1))
			/ HISTOGRAM_WORKGROUP_SIZE;

		command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, histogram_pipeline);
		command_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eCompute,
			histogram_pipeline_layout,
			0,
			{resource_set.histogram_descriptor_set},
			{}
		);
		command_buffer.dispatch(histogram_group_count.x, histogram_group_count.y, histogram_group_count.z);

		/*===== Post-Histogram Sync =====*/

		const auto histogram_barrier = vk::MemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
			.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eComputeShader,
			.dstAccessMask = vk::AccessFlagBits2::eShaderRead
		};
		command_buffer.pipelineBarrier2(vk::DependencyInfo().setMemoryBarriers(histogram_barrier));

		/*===== Reduce =====*/

		command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, reduce_pipeline);
		command_buffer.bindDescriptorSets(
			vk::PipelineBindPoint::eCompute,
			reduce_pipeline_layout,
			0,
			{resource_set.reduce_descriptor_set},
			{}
		);
		command_buffer.dispatch(1, 1, 1);

		/*===== Post-reduce Sync =====*/

		const auto reduce_barrier = vk::MemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eComputeShader,
			.srcAccessMask = vk::AccessFlagBits2::eShaderWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eFragmentShader,
			.dstAccessMask = vk::AccessFlagBits2::eShaderRead
		};
		command_buffer.pipelineBarrier2(vk::DependencyInfo().setMemoryBarriers(reduce_barrier));
	}

	std::expected<AutoExposurePipeline, Error> AutoExposurePipeline::create(
		const vulkan::DeviceContext& context
	) noexcept
	{
		/*===== Descriptor set layout =====*/

		auto clear_resource_layout_result =
			context.device
				.createDescriptorSetLayout(
					vk::DescriptorSetLayoutCreateInfo().setBindings(CLEAR_RESOURCE_BINDINGS)
				)
				.transform_error(Error::from<vk::Result>());
		auto histogram_resource_layout_result =
			context.device
				.createDescriptorSetLayout(
					vk::DescriptorSetLayoutCreateInfo().setBindings(HISTOGRAM_RESOURCE_BINDINGS)
				)
				.transform_error(Error::from<vk::Result>());
		auto reduce_resource_layout_result =
			context.device
				.createDescriptorSetLayout(
					vk::DescriptorSetLayoutCreateInfo().setBindings(REDUCE_RESOURCE_BINDINGS)
				)
				.transform_error(Error::from<vk::Result>());

		if (!clear_resource_layout_result)
		{
			return clear_resource_layout_result.error().forward(
				"Create resource layout for 'clear' program failed"
			);
		}
		if (!histogram_resource_layout_result)
		{
			return histogram_resource_layout_result.error().forward(
				"Create resource layout for 'histogram' program failed"
			);
		}
		if (!reduce_resource_layout_result)
		{
			return reduce_resource_layout_result.error().forward(
				"Create resource layout for 'reduce' program failed"
			);
		}

		auto clear_resource_layout = std::move(*clear_resource_layout_result);
		auto histogram_resource_layout = std::move(*histogram_resource_layout_result);
		auto reduce_resource_layout = std::move(*reduce_resource_layout_result);

		/*===== Pipeline layout =====*/

		const auto clear_resource_layout_list =
			std::to_array<vk::DescriptorSetLayout>({clear_resource_layout});
		const auto histogram_resource_layout_list =
			std::to_array<vk::DescriptorSetLayout>({histogram_resource_layout});
		const auto reduce_resource_layout_list =
			std::to_array<vk::DescriptorSetLayout>({reduce_resource_layout});

		auto clear_pipeline_layout_result =
			context.device
				.createPipelineLayout(
					vk::PipelineLayoutCreateInfo().setSetLayouts(clear_resource_layout_list)
				)
				.transform_error(Error::from<vk::Result>());
		auto histogram_pipeline_layout_result =
			context.device
				.createPipelineLayout(
					vk::PipelineLayoutCreateInfo().setSetLayouts(histogram_resource_layout_list)
				)
				.transform_error(Error::from<vk::Result>());
		auto reduce_pipeline_layout_result =
			context.device
				.createPipelineLayout(
					vk::PipelineLayoutCreateInfo().setSetLayouts(reduce_resource_layout_list)
				)
				.transform_error(Error::from<vk::Result>());

		if (!clear_pipeline_layout_result)
		{
			return clear_pipeline_layout_result.error().forward(
				"Create pipeline layout for 'clear' program failed"
			);
		}
		if (!histogram_pipeline_layout_result)
		{
			return histogram_pipeline_layout_result.error().forward(
				"Create pipeline layout for 'histogram' program failed"
			);
		}
		if (!reduce_pipeline_layout_result)
		{
			return reduce_pipeline_layout_result.error().forward(
				"Create pipeline layout for 'reduce' program failed"
			);
		}

		auto clear_pipeline_layout = std::move(*clear_pipeline_layout_result);
		auto histogram_pipeline_layout = std::move(*histogram_pipeline_layout_result);
		auto reduce_pipeline_layout = std::move(*reduce_pipeline_layout_result);

		/*===== Shader modules =====*/

		auto clear_shader_result = vulkan::create_shader(context.device, shader::auto_exposure::clear);
		auto histogram_shader_result =
			vulkan::create_shader(context.device, shader::auto_exposure::histogram);
		auto reduce_shader_result = vulkan::create_shader(context.device, shader::auto_exposure::reduce);

		if (!clear_shader_result)
		{
			return clear_shader_result.error().forward("Create shader for 'clear' program failed");
		}
		if (!histogram_shader_result)
		{
			return histogram_shader_result.error().forward("Create shader for 'histogram' program failed");
		}
		if (!reduce_shader_result)
		{
			return reduce_shader_result.error().forward("Create shader for 'reduce' program failed");
		}

		auto clear_shader = std::move(*clear_shader_result);
		auto histogram_shader = std::move(*histogram_shader_result);
		auto reduce_shader = std::move(*reduce_shader_result);

		/*===== Shader stage info =====*/

		const auto clear_shader_stage_info = vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eCompute,
			.module = clear_shader,
			.pName = "main"
		};
		const auto histogram_shader_stage_info = vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eCompute,
			.module = histogram_shader,
			.pName = "main"
		};
		const auto reduce_shader_stage_info = vk::PipelineShaderStageCreateInfo{
			.stage = vk::ShaderStageFlagBits::eCompute,
			.module = reduce_shader,
			.pName = "main"
		};

		/*===== Pipeline create =====*/

		const auto clear_pipeline_create_info = vk::ComputePipelineCreateInfo{
			.stage = clear_shader_stage_info,
			.layout = clear_pipeline_layout,
		};
		const auto histogram_pipeline_create_info = vk::ComputePipelineCreateInfo{
			.stage = histogram_shader_stage_info,
			.layout = histogram_pipeline_layout,
		};
		const auto reduce_pipeline_create_info = vk::ComputePipelineCreateInfo{
			.stage = reduce_shader_stage_info,
			.layout = reduce_pipeline_layout,
		};

		auto clear_pipeline_result =
			context.device.createComputePipeline(nullptr, clear_pipeline_create_info)
				.transform_error(Error::from<vk::Result>());
		auto histogram_pipeline_result =
			context.device.createComputePipeline(nullptr, histogram_pipeline_create_info)
				.transform_error(Error::from<vk::Result>());
		auto reduce_pipeline_result =
			context.device.createComputePipeline(nullptr, reduce_pipeline_create_info)
				.transform_error(Error::from<vk::Result>());

		if (!clear_pipeline_result)
		{
			return clear_pipeline_result.error().forward(
				"Create compute pipeline for 'clear' program failed"
			);
		}
		if (!histogram_pipeline_result)
		{
			return histogram_pipeline_result.error().forward(
				"Create compute pipeline for 'histogram' program failed"
			);
		}
		if (!reduce_pipeline_result)
		{
			return reduce_pipeline_result.error().forward(
				"Create compute pipeline for 'reduce' program failed"
			);
		}

		auto clear_pipeline = std::move(*clear_pipeline_result);
		auto histogram_pipeline = std::move(*histogram_pipeline_result);
		auto reduce_pipeline = std::move(*reduce_pipeline_result);

		/*===== Sampler =====*/

		constexpr auto input_sampler_create_info = vk::SamplerCreateInfo{
			.magFilter = vk::Filter::eNearest,
			.minFilter = vk::Filter::eNearest,
			.mipmapMode = vk::SamplerMipmapMode::eNearest,
			.addressModeU = vk::SamplerAddressMode::eClampToEdge,
			.addressModeV = vk::SamplerAddressMode::eClampToEdge,
			.addressModeW = vk::SamplerAddressMode::eClampToEdge,
			.mipLodBias = 0.0f,
			.minLod = 0.0f,
			.maxLod = 0.0f,
		};

		auto input_sampler_result =
			context.device.createSampler(input_sampler_create_info)
				.transform_error(Error::from<vk::Result>());
		if (!input_sampler_result) return input_sampler_result.error().forward("Create input sampler failed");
		auto input_sampler = std::move(*input_sampler_result);

		constexpr auto mask_sampler_create_info = vk::SamplerCreateInfo{
			.magFilter = vk::Filter::eLinear,
			.minFilter = vk::Filter::eLinear,
			.mipmapMode = vk::SamplerMipmapMode::eNearest,
			.addressModeU = vk::SamplerAddressMode::eClampToEdge,
			.addressModeV = vk::SamplerAddressMode::eClampToEdge,
			.addressModeW = vk::SamplerAddressMode::eClampToEdge,
			.mipLodBias = 0.0f,
			.minLod = 0.0f,
			.maxLod = 0.0f,
		};

		auto mask_sampler_result =
			context.device.createSampler(mask_sampler_create_info).transform_error(Error::from<vk::Result>());
		if (!mask_sampler_result) return mask_sampler_result.error().forward("Create mask sampler failed");
		auto mask_sampler = std::move(*mask_sampler_result);

		return AutoExposurePipeline(
			std::move(clear_resource_layout),
			std::move(histogram_resource_layout),
			std::move(reduce_resource_layout),
			std::move(clear_pipeline_layout),
			std::move(histogram_pipeline_layout),
			std::move(reduce_pipeline_layout),
			std::move(clear_pipeline),
			std::move(histogram_pipeline),
			std::move(reduce_pipeline),
			std::move(input_sampler),
			std::move(mask_sampler)
		);
	}

	std::expected<std::vector<AutoExposurePipeline::ResourceSet>, Error> AutoExposurePipeline::
		create_resource_sets(const vulkan::DeviceContext& context, uint32_t count) const noexcept
	{
		const auto bindings = util::array_concat(
			CLEAR_RESOURCE_BINDINGS,
			HISTOGRAM_RESOURCE_BINDINGS,
			REDUCE_RESOURCE_BINDINGS
		);
		const auto pool_sizes = vulkan::calc_pool_sizes(bindings, count);

		auto descriptor_pool_result =
			context.device
				.createDescriptorPool(
					vk::DescriptorPoolCreateInfo()
						.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet)
						.setMaxSets(count * 3)
						.setPoolSizes(pool_sizes)
				)
				.transform_error(Error::from<vk::Result>());
		if (!descriptor_pool_result)
		{
			return descriptor_pool_result.error().forward(
				"Create descriptor pool for auto exposure resource sets failed"
			);
		}
		auto descriptor_pool = std::make_shared<vk::raii::DescriptorPool>(std::move(*descriptor_pool_result));

		const auto clear_layouts = std::vector(count, *clear_resource_layout);
		const auto histogram_layouts = std::vector(count, *histogram_resource_layout);
		const auto reduce_layouts = std::vector(count, *reduce_resource_layout);

		const auto clear_set_alloc_info =
			vk::DescriptorSetAllocateInfo().setDescriptorPool(*descriptor_pool).setSetLayouts(clear_layouts);
		const auto histogram_set_alloc_info =
			vk::DescriptorSetAllocateInfo()
				.setDescriptorPool(*descriptor_pool)
				.setSetLayouts(histogram_layouts);
		const auto reduce_set_alloc_info =
			vk::DescriptorSetAllocateInfo().setDescriptorPool(*descriptor_pool).setSetLayouts(reduce_layouts);

		auto clear_sets_result =
			context.device.allocateDescriptorSets(clear_set_alloc_info)
				.transform_error(Error::from<vk::Result>());
		auto histogram_sets_result =
			context.device.allocateDescriptorSets(histogram_set_alloc_info)
				.transform_error(Error::from<vk::Result>());
		auto reduce_sets_result =
			context.device.allocateDescriptorSets(reduce_set_alloc_info)
				.transform_error(Error::from<vk::Result>());

		if (!clear_sets_result)
		{
			return clear_sets_result.error().forward(
				"Allocate descriptor sets for 'clear' program resource sets failed"
			);
		}
		if (!histogram_sets_result)
		{
			return histogram_sets_result.error().forward(
				"Allocate descriptor sets for 'histogram' program resource sets failed"
			);
		}
		if (!reduce_sets_result)
		{
			return reduce_sets_result.error().forward(
				"Allocate descriptor sets for 'reduce' program resource sets failed"
			);
		}

		auto clear_sets = std::move(*clear_sets_result);
		auto histogram_sets = std::move(*histogram_sets_result);
		auto reduce_sets = std::move(*reduce_sets_result);

		return std::views::zip_transform(
				   CTOR_LAMBDA(ResourceSet),
				   std::views::repeat(descriptor_pool),
				   clear_sets | std::views::as_rvalue,
				   histogram_sets | std::views::as_rvalue,
				   reduce_sets | std::views::as_rvalue,
				   std::views::repeat(*input_sampler),
				   std::views::repeat(*mask_sampler)
			   )
			| std::ranges::to<std::vector>();
	}

	void AutoExposurePipeline::ResourceSet::update(
		const vulkan::DeviceContext& context,
		const vulkan::ElementBufferRef<ExposureParam>& exposure_param,
		const AutoExposureResource& resource,
		const AutoExposureResource& prev_resource,
		vk::ImageView mask_image_view,
		vk::ImageView input_image_view,
		glm::u32vec2 image_size
	) noexcept
	{
		this->image_size = image_size;

		/*===== Resource Infos =====*/

		const auto histogram_buffer_info = vk::DescriptorBufferInfo{
			.buffer = resource->histogram_buffer,
			.offset = 0,
			.range = vk::WholeSize
		};
		const auto exposure_param_buffer_info = vk::DescriptorBufferInfo{
			.buffer = exposure_param,
			.offset = 0,
			.range = vk::WholeSize,
		};
		const auto exposure_result_buffer_info = vk::DescriptorBufferInfo{
			.buffer = resource->exposure_result_buffer,
			.offset = 0,
			.range = vk::WholeSize
		};
		const auto exposure_frame_buffer_info = vk::DescriptorBufferInfo{
			.buffer = resource->exposure_frame_buffer,
			.offset = 0,
			.range = vk::WholeSize
		};
		const auto last_exposure_frame_buffer_info = vk::DescriptorBufferInfo{
			.buffer = prev_resource->exposure_frame_buffer,
			.offset = 0,
			.range = vk::WholeSize
		};
		const auto input_image_info = vk::DescriptorImageInfo{
			.sampler = input_sampler,
			.imageView = input_image_view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};
		const auto mask_image_info = vk::DescriptorImageInfo{
			.sampler = mask_sampler,
			.imageView = mask_image_view,
			.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
		};

		/*===== Write Infos =====*/

		const auto clear_binding_0 = vk::WriteDescriptorSet{
			.dstSet = clear_descriptor_set,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.pBufferInfo = &histogram_buffer_info
		};

		const auto histogram_binding_0 = vk::WriteDescriptorSet{
			.dstSet = histogram_descriptor_set,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &exposure_param_buffer_info
		};
		const auto histogram_binding_1 = vk::WriteDescriptorSet{
			.dstSet = histogram_descriptor_set,
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &input_image_info
		};
		const auto histogram_binding_2 = vk::WriteDescriptorSet{
			.dstSet = histogram_descriptor_set,
			.dstBinding = 2,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.pBufferInfo = &histogram_buffer_info
		};
		const auto histogram_binding_3 = vk::WriteDescriptorSet{
			.dstSet = histogram_descriptor_set,
			.dstBinding = 3,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.pImageInfo = &mask_image_info
		};
		const auto reduce_binding_0 = vk::WriteDescriptorSet{
			.dstSet = reduce_descriptor_set,
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.pBufferInfo = &exposure_param_buffer_info
		};
		const auto reduce_binding_1 = vk::WriteDescriptorSet{
			.dstSet = reduce_descriptor_set,
			.dstBinding = 1,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.pBufferInfo = &histogram_buffer_info
		};
		const auto reduce_binding_2 = vk::WriteDescriptorSet{
			.dstSet = reduce_descriptor_set,
			.dstBinding = 2,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.pBufferInfo = &exposure_result_buffer_info
		};
		const auto reduce_binding_3 = vk::WriteDescriptorSet{
			.dstSet = reduce_descriptor_set,
			.dstBinding = 3,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.pBufferInfo = &last_exposure_frame_buffer_info
		};
		const auto reduce_binding_4 = vk::WriteDescriptorSet{
			.dstSet = reduce_descriptor_set,
			.dstBinding = 4,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.pBufferInfo = &exposure_frame_buffer_info
		};

		const auto descriptor_writes = std::to_array({
			clear_binding_0,
			histogram_binding_0,
			histogram_binding_1,
			histogram_binding_2,
			histogram_binding_3,
			reduce_binding_0,
			reduce_binding_1,
			reduce_binding_2,
			reduce_binding_3,
			reduce_binding_4,
		});

		context.device.updateDescriptorSets(descriptor_writes, {});
	}
}

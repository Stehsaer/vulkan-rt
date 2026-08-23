#pragma once

#include "common/number-literals.hpp"
#include "common/util/error.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/util/shader.hpp"

#include <array>
#include <concepts>
#include <cstddef>
#include <expected>
#include <glm/ext/vector_uint3_sized.hpp>
#include <span>
#include <type_traits>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief Compute pipeline object
	///
	/// @tparam PushConstant Push constant type
	/// @tparam BlockSize Block size of the pipeline
	///
	template <typename PushConstant, glm::u32vec3 BlockSize>
		requires(std::same_as<PushConstant, void> || std::is_trivially_copyable_v<PushConstant>)
	class ComputePipeline
	{
	  public:

		///
		/// @brief Create a compute pipeline object
		///
		/// @tparam DescriptorSetCount Count of descriptor sets
		/// @param context Vulkan context
		/// @param set_layouts List of descriptor set layouts
		/// @param shader_data Binary shader data
		/// @param shader_entry Shader entry name, defaults to `"main"`
		/// @return Created pipeline or error
		///
		template <size_t DescriptorSetCount>
		[[nodiscard]]
		static std::expected<ComputePipeline, Error> create(
			const vulkan::Context& context,
			std::array<vk::DescriptorSetLayout, DescriptorSetCount> set_layouts,
			std::span<const std::byte> shader_data,
			const char* shader_entry = "main"
		) noexcept
		{
			const auto push_constant_range = vk::PushConstantRange{
				.stageFlags = vk::ShaderStageFlagBits::eCompute,
				.offset = 0,
				.size = sizeof(PushConstant),
			};

			const auto pipeline_layout_info =
				vk::PipelineLayoutCreateInfo()
					.setSetLayouts(set_layouts)
					.setPushConstantRanges(push_constant_range);

			auto pipeline_layout_result = context.device.createPipelineLayout(pipeline_layout_info);
			if (!pipeline_layout_result) return Error::from(pipeline_layout_result);
			auto pipeline_layout = std::move(*pipeline_layout_result);

			auto shader_result = create_shader(context.device, shader_data);
			if (!shader_result) return shader_result.error().forward("Create shader module failed");
			auto shader = std::move(*shader_result);

			const auto shader_info = vk::PipelineShaderStageCreateInfo{
				.stage = vk::ShaderStageFlagBits::eCompute,
				.module = shader,
				.pName = shader_entry,
			};

			const auto pipeline_create_info = vk::ComputePipelineCreateInfo{
				.stage = shader_info,
				.layout = pipeline_layout,
			};
			auto pipeline_result = context.device.createComputePipeline(nullptr, pipeline_create_info);
			if (!pipeline_result) return Error::from(pipeline_result);
			auto pipeline = std::move(*pipeline_result);

			return ComputePipeline(std::move(pipeline_layout), std::move(pipeline));
		}

		///
		/// @brief Create a compute pipeline object
		///
		/// @param context Vulkan context
		/// @param set_layout Descriptor set layout for the first (and only) set
		/// @param shader Shader binary
		/// @param shader_entry Shader entry name, defaults to `"main"`
		/// @return Created pipeline or error
		///
		static std::expected<ComputePipeline, Error> create(
			const vulkan::Context& context,
			vk::DescriptorSetLayout set_layout,
			std::span<const std::byte> shader,
			const char* shader_entry = "main"
		) noexcept
		{
			return create(context, std::to_array({set_layout}), shader, shader_entry);
		}

		///
		/// @brief Dispatch the compute pipeline
		///
		/// @tparam DescriptorSetCount Count of descriptors
		/// @param command_buffer Command buffer
		/// @param sets List of descriptor set to use
		/// @param push_constant Push constant value
		/// @param thread_count Dimension of threads to dispatch
		///
		template <size_t DescriptorSetCount>
		void dispatch(
			const vk::raii::CommandBuffer& command_buffer,
			std::array<vk::DescriptorSet, DescriptorSetCount> sets,
			const PushConstant& push_constant,
			glm::u32vec3 thread_count
		) const noexcept
		{
			const auto dispatch_size = (thread_count + BlockSize - 1_u32) / BlockSize;

			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
			command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0, sets, {});
			command_buffer
				.pushConstants<PushConstant>(layout, vk::ShaderStageFlagBits::eCompute, 0, push_constant);
			command_buffer.dispatch(dispatch_size.x, dispatch_size.y, dispatch_size.z);
		}

		///
		/// @brief Dispatch the compute pipeline
		///
		/// @param command_buffer Command buffer
		/// @param set Descriptor set for the first and only slot
		/// @param push_constant Push constant value
		/// @param thread_count Dimension of threads to dispatch
		///
		void dispatch(
			const vk::raii::CommandBuffer& command_buffer,
			vk::DescriptorSet set,
			const PushConstant& push_constant,
			glm::u32vec3 thread_count
		) const noexcept
		{
			dispatch(command_buffer, std::to_array({set}), push_constant, thread_count);
		}

	  private:

		vk::raii::PipelineLayout layout;
		vk::raii::Pipeline pipeline;

		explicit ComputePipeline(vk::raii::PipelineLayout layout, vk::raii::Pipeline pipeline) :
			layout(std::move(layout)),
			pipeline(std::move(pipeline))
		{}

	  public:

		ComputePipeline(const ComputePipeline&) = delete;
		ComputePipeline(ComputePipeline&&) = default;
		ComputePipeline& operator=(const ComputePipeline&) = delete;
		ComputePipeline& operator=(ComputePipeline&&) = default;
	};

	///
	/// @brief Compute pipeline object without push constant
	///
	/// @tparam BlockSize Block size of the pipeline
	///
	template <glm::u32vec3 BlockSize>
	class ComputePipeline<void, BlockSize>
	{
	  public:

		///
		/// @brief Create a compute pipeline object
		///
		/// @tparam DescriptorSetCount Count of descriptor sets
		/// @param context Vulkan context
		/// @param set_layouts List of descriptor set layouts
		/// @param shader_data Binary shader data
		/// @param shader_entry Shader entry name, defaults to `"main"`
		/// @return Created pipeline or error
		///
		template <size_t DescriptorSetCount>
		[[nodiscard]]
		static std::expected<ComputePipeline, Error> create(
			const vulkan::Context& context,
			std::array<vk::DescriptorSetLayout, DescriptorSetCount> set_layouts,
			std::span<const std::byte> shader_data,
			const char* shader_entry = "main"
		) noexcept
		{
			const auto pipeline_layout_info = vk::PipelineLayoutCreateInfo().setSetLayouts(set_layouts);

			auto pipeline_layout_result = context.device.createPipelineLayout(pipeline_layout_info);
			if (!pipeline_layout_result) return Error::from(pipeline_layout_result);
			auto pipeline_layout = std::move(*pipeline_layout_result);

			auto shader_result = create_shader(context.device, shader_data);
			if (!shader_result) return shader_result.error().forward("Create shader module failed");
			auto shader = std::move(*shader_result);

			const auto shader_info = vk::PipelineShaderStageCreateInfo{
				.stage = vk::ShaderStageFlagBits::eCompute,
				.module = shader,
				.pName = shader_entry,
			};

			const auto pipeline_create_info = vk::ComputePipelineCreateInfo{
				.stage = shader_info,
				.layout = pipeline_layout,
			};
			auto pipeline_result = context.device.createComputePipeline(nullptr, pipeline_create_info);
			if (!pipeline_result) return Error::from(pipeline_result);
			auto pipeline = std::move(*pipeline_result);

			return ComputePipeline(std::move(pipeline_layout), std::move(pipeline));
		}

		///
		/// @brief Create a compute pipeline object
		///
		/// @param context Vulkan context
		/// @param set_layout Descriptor set layout for the first (and only) set
		/// @param shader Shader binary
		/// @param shader_entry Shader entry name, defaults to `"main"`
		/// @return Created pipeline or error
		///
		static std::expected<ComputePipeline, Error> create(
			const vulkan::Context& context,
			vk::DescriptorSetLayout set_layout,
			std::span<const std::byte> shader,
			const char* shader_entry = "main"
		) noexcept
		{
			return create(context, std::to_array({set_layout}), shader, shader_entry);
		}

		///
		/// @brief Dispatch the compute pipeline
		///
		/// @tparam DescriptorSetCount Count of descriptors
		/// @param command_buffer Command buffer
		/// @param sets List of descriptor set to use
		/// @param thread_count Dimension of threads to dispatch
		///
		template <size_t DescriptorSetCount>
		void dispatch(
			const vk::raii::CommandBuffer& command_buffer,
			std::array<vk::DescriptorSet, DescriptorSetCount> sets,
			glm::u32vec3 thread_count
		) const noexcept
		{
			const auto dispatch_size = (thread_count + BlockSize - 1_u32) / BlockSize;

			command_buffer.bindPipeline(vk::PipelineBindPoint::eCompute, pipeline);
			command_buffer.bindDescriptorSets(vk::PipelineBindPoint::eCompute, layout, 0, sets, {});
			command_buffer.dispatch(dispatch_size.x, dispatch_size.y, dispatch_size.z);
		}

		///
		/// @brief Dispatch the compute pipeline
		///
		/// @param command_buffer Command buffer
		/// @param set Descriptor set for the first and only slot
		/// @param thread_count Dimension of threads to dispatch
		///
		void dispatch(
			const vk::raii::CommandBuffer& command_buffer,
			vk::DescriptorSet set,
			glm::u32vec3 thread_count
		) const noexcept
		{
			dispatch(command_buffer, std::to_array({set}), thread_count);
		}

	  private:

		vk::raii::PipelineLayout layout;
		vk::raii::Pipeline pipeline;

		explicit ComputePipeline(vk::raii::PipelineLayout layout, vk::raii::Pipeline pipeline) :
			layout(std::move(layout)),
			pipeline(std::move(pipeline))
		{}

	  public:

		ComputePipeline(const ComputePipeline&) = delete;
		ComputePipeline(ComputePipeline&&) = default;
		ComputePipeline& operator=(const ComputePipeline&) = delete;
		ComputePipeline& operator=(ComputePipeline&&) = default;
	};
}

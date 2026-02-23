#include "interface/camera-param.hpp"

#include "common/util/error.hpp"
#include "common/util/span.hpp"
#include <vulkan/vulkan.hpp>

namespace interface
{
	namespace
	{
		constexpr auto camera_param_binding = vk::DescriptorSetLayoutBinding{
			.binding = 0,
			.descriptorType = vk::DescriptorType::eUniformBuffer,
			.descriptorCount = 1,
			.stageFlags = vk::ShaderStageFlagBits::eVertex | vk::ShaderStageFlagBits::eFragment
		};

		constexpr auto bindings = std::to_array<vk::DescriptorSetLayoutBinding>({
			camera_param_binding,
		});

		constexpr auto get_pool_sizes(uint32_t set_count) noexcept
		{
			return std::to_array<vk::DescriptorPoolSize>({
				{
                 .type = vk::DescriptorType::eUniformBuffer,
                 .descriptorCount = set_count,
				 },
			});
		}
	}

	std::expected<CameraParam::Layout, Error> CameraParam::Layout::create(
		const vk::raii::Device& device
	) noexcept
	{
		const auto layout_create_info = vk::DescriptorSetLayoutCreateInfo().setBindings(bindings);

		auto layout_result =
			device.createDescriptorSetLayout(layout_create_info).transform_error(Error::from<vk::Result>());
		if (!layout_result) return layout_result.error().forward("Create descriptor set layout failed");
		auto layout = std::move(*layout_result);

		return Layout{.layout = std::move(layout)};
	}

	std::expected<CameraParam::Resource, Error> CameraParam::Resource::create(
		const vk::raii::Device& device [[maybe_unused]],
		const vulkan::alloc::Allocator& allocator
	) noexcept
	{
		const auto staging_buffer_create_info =
			vk::BufferCreateInfo{.size = sizeof(CameraParam), .usage = vk::BufferUsageFlagBits::eTransferSrc};

		const auto buffer_create_info = vk::BufferCreateInfo{
			.size = sizeof(CameraParam),
			.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst
		};

		auto staging_buffer_result =
			allocator.create_buffer(staging_buffer_create_info, vulkan::alloc::MemoryUsage::CpuToGpu);
		if (!staging_buffer_result)
			return staging_buffer_result.error().forward("Create staging buffer failed");
		auto staging_buffer = std::move(*staging_buffer_result);

		auto buffer_result = allocator.create_buffer(buffer_create_info, vulkan::alloc::MemoryUsage::GpuOnly);
		if (!buffer_result) return buffer_result.error().forward("Create buffer failed");
		auto buffer = std::move(*buffer_result);

		return Resource{.staging_buffer = std::move(staging_buffer), .buffer = std::move(buffer)};
	}

	std::expected<void, Error> CameraParam::Resource::update(const CameraParam& param) const noexcept
	{
		if (const auto result = staging_buffer.upload(util::object_as_bytes(param)); !result)
			return result.error().forward("Upload data to staging buffer failed");

		return {};
	}

	void CameraParam::Resource::upload(const vk::raii::CommandBuffer& command_buffer) const noexcept
	{
		const auto copy_region = vk::BufferCopy{.srcOffset = 0, .dstOffset = 0, .size = sizeof(CameraParam)};
		command_buffer.copyBuffer(*staging_buffer, *buffer, copy_region);
	}

	vk::BufferMemoryBarrier2 CameraParam::Resource::barrier_after_upload() const noexcept
	{
		return vk::BufferMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.dstStageMask =
				vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eFragmentShader,
			.dstAccessMask = vk::AccessFlagBits2::eUniformRead,
			.buffer = *buffer,
			.offset = 0,
			.size = vk::WholeSize
		};
	}

	void CameraParam::DescriptorSet::bind_resource(
		const vk::raii::Device& device,
		const Resource& resource
	) const noexcept
	{
		const auto buffer_info =
			vk::DescriptorBufferInfo{.buffer = *resource.buffer, .offset = 0, .range = sizeof(CameraParam)};
		const auto write_descriptor_set = vk::WriteDescriptorSet{
			.dstSet = descriptor_set,
			.dstBinding = camera_param_binding.binding,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = camera_param_binding.descriptorType,
			.pBufferInfo = &buffer_info
		};

		device.updateDescriptorSets(write_descriptor_set, {});
	}

	std::expected<CameraParam::DescriptorPool, Error> CameraParam::DescriptorPool::create(
		const vk::raii::Device& device,
		const Layout& layout,
		uint32_t set_count
	) noexcept
	{
		const auto pool_sizes = get_pool_sizes(set_count);
		const auto descriptor_set_layouts = std::vector<vk::DescriptorSetLayout>(set_count, *layout.layout);
		const auto descriptor_pool_create_info =
			vk::DescriptorPoolCreateInfo()
				.setMaxSets(set_count)
				.setPoolSizes(pool_sizes)
				.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);

		auto pool_result =
			device.createDescriptorPool(descriptor_pool_create_info)
				.transform_error(Error::from<vk::Result>());
		if (!pool_result) return pool_result.error().forward("Create descriptor pool failed");
		auto pool = std::move(*pool_result);

		auto sets_result =
			device
				.allocateDescriptorSets(
					vk::DescriptorSetAllocateInfo{.descriptorPool = pool, .descriptorSetCount = set_count}
						.setSetLayouts(descriptor_set_layouts)
				)
				.transform_error(Error::from<vk::Result>());
		if (!sets_result) return sets_result.error().forward("Allocate descriptor sets failed");
		auto sets = std::move(*sets_result);

		return DescriptorPool(std::move(pool), std::move(sets));
	}

	std::vector<CameraParam::DescriptorSet> CameraParam::DescriptorPool::get_descriptor_sets() const
	{
		return sets
			| std::views::transform([](const auto& set) { return DescriptorSet{.descriptor_set = set}; })
			| std::ranges::to<std::vector>();
	}
}

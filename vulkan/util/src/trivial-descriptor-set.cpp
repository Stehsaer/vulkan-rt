#include "vulkan/util/trivial-descriptor-set.hpp"
#include "common/util/pmr-unique-ptr.hpp"

#include <libassert/assert.hpp>
#include <memory_resource>
#include <vulkan/vulkan.hpp>

namespace vulkan::trivset
{
	namespace detail
	{
		ImageWriteInfo::ImageWriteInfo(
			WriteTarget write_target,
			std::pmr::memory_resource& resource,
			vk::DescriptorType type,
			const vk::DescriptorImageInfo& info
		) noexcept :
			WriteTarget(write_target),
			type(type),
			info(util::PmrUniquePtr<vk::DescriptorImageInfo>::from(resource, info))
		{}

		BufferWriteInfo::BufferWriteInfo(
			WriteTarget write_target,
			std::pmr::memory_resource& resource,
			vk::DescriptorType type,
			const vk::DescriptorBufferInfo& info
		) noexcept :
			WriteTarget(write_target),
			type(type),
			info(util::PmrUniquePtr<vk::DescriptorBufferInfo>::from(resource, info))
		{}

		vk::WriteDescriptorSet ImageWriteInfo::operator()() const noexcept
		{
			return vk::WriteDescriptorSet{
				.dstSet = set,
				.dstBinding = binding,
				.descriptorCount = 1,
				.descriptorType = type,
				.pImageInfo = info.get(),
			};
		}

		vk::WriteDescriptorSet BufferWriteInfo::operator()() const noexcept
		{
			return vk::WriteDescriptorSet{
				.dstSet = set,
				.dstBinding = binding,
				.descriptorCount = 1,
				.descriptorType = type,
				.pBufferInfo = info.get(),
			};
		}

		AccelerationStructureWriteInfo::AccelerationStructureWriteInfo(
			WriteTarget write_target,
			std::pmr::memory_resource& resource,
			vk::AccelerationStructureKHR accel_struct
		) noexcept :
			WriteTarget(write_target),
			accel_struct(util::PmrUniquePtr<vk::AccelerationStructureKHR>::from(resource, accel_struct)),
			write_info(
				util::PmrUniquePtr<vk::WriteDescriptorSetAccelerationStructureKHR>::from(
					resource,
					vk::WriteDescriptorSetAccelerationStructureKHR{
						.accelerationStructureCount = 1,
						.pAccelerationStructures = this->accel_struct.get()
					}
				)
			)
		{}

		vk::WriteDescriptorSet AccelerationStructureWriteInfo::operator()() const noexcept
		{
			return vk::WriteDescriptorSet{
				.pNext = write_info.get(),
				.dstSet = set,
				.dstBinding = binding,
				.descriptorCount = 1,
				.descriptorType = vk::DescriptorType::eAccelerationStructureKHR
			};
		}
	}

	namespace slot
	{
		using namespace detail;

		ImageWriteInfo CombinedImageSampler::write_info_fn(
			std::pmr::memory_resource& resource,
			const WriteTarget& info
		) const noexcept
		{
			DEBUG_ASSERT(sampler != nullptr);
			DEBUG_ASSERT(image != nullptr);

			return ImageWriteInfo(
				info,
				resource,
				TYPE,
				{
					.sampler = sampler,
					.imageView = image,
					.imageLayout =
						layout_general ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal,
				}
			);
		}

		ImageWriteInfo SampledImage::write_info_fn(
			std::pmr::memory_resource& resource,
			const WriteTarget& info
		) const noexcept
		{
			DEBUG_ASSERT(image != nullptr);

			return ImageWriteInfo(
				info,
				resource,
				TYPE,
				{
					.sampler = nullptr,
					.imageView = image,
					.imageLayout =
						layout_general ? vk::ImageLayout::eGeneral : vk::ImageLayout::eShaderReadOnlyOptimal,
				}
			);
		}

		ImageWriteInfo Sampler::write_info_fn(
			std::pmr::memory_resource& resource,
			const WriteTarget& info
		) const noexcept
		{
			DEBUG_ASSERT(sampler != nullptr);

			return ImageWriteInfo(info, resource, TYPE, {.sampler = sampler});
		}

		ImageWriteInfo StorageImage::write_info_fn(
			std::pmr::memory_resource& resource,
			const WriteTarget& info
		) const noexcept
		{
			DEBUG_ASSERT(image != nullptr);

			return ImageWriteInfo(
				info,
				resource,
				TYPE,
				{
					.sampler = nullptr,
					.imageView = image,
					.imageLayout = vk::ImageLayout::eGeneral,
				}
			);
		}

		BufferWriteInfo UniformBuffer::write_info_fn(
			std::pmr::memory_resource& resource,
			const WriteTarget& info
		) const noexcept
		{
			DEBUG_ASSERT(buffer != nullptr);

			return BufferWriteInfo(
				info,
				resource,
				TYPE,
				{
					.buffer = buffer,
					.offset = offset,
					.range = size,
				}
			);
		}

		BufferWriteInfo StorageBuffer::write_info_fn(
			std::pmr::memory_resource& resource,
			const WriteTarget& info
		) const noexcept
		{
			DEBUG_ASSERT(buffer != nullptr);

			return BufferWriteInfo(
				info,
				resource,
				TYPE,
				{
					.buffer = buffer,
					.offset = offset,
					.range = size,
				}
			);
		}

		AccelerationStructureWriteInfo AccelerationStructure::write_info_fn(
			std::pmr::memory_resource& resource,
			const WriteTarget& info
		) const noexcept
		{
			DEBUG_ASSERT(accel_struct != nullptr);

			return AccelerationStructureWriteInfo(info, resource, accel_struct);
		}
	}
}

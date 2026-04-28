#pragma once

#include "dyn-buffer.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/alloc/buffer.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief Two-stage host-uploadable buffer. Holds a single item of type T.
	///
	/// @tparam T Type of the buffer data
	///
	template <typename T>
	class StagedBuffer
	{
	  public:

		///
		/// @brief Create a staged buffer with the given usage flags
		///
		/// @param device_context Vulkan device
		/// @param usage_flags Buffer usage flags. `eTransferDst` is automatically added
		/// @return Created StagedBuffer, or error if creation failed
		///
		[[nodiscard]]
		static std::expected<StagedBuffer, Error> create(
			const DeviceContext& device_context,
			vk::BufferUsageFlags usage_flags
		) noexcept
		{
			auto staging_buffer_result = device_context.allocator.create_element_buffer<T>(
				vk::BufferUsageFlagBits::eTransferSrc,
				vulkan::MemoryUsage::CpuToGpu
			);
			auto device_buffer_result = device_context.allocator.create_element_buffer<T>(
				usage_flags | vk::BufferUsageFlagBits::eTransferDst,
				vulkan::MemoryUsage::GpuOnly
			);

			if (!staging_buffer_result)
				return staging_buffer_result.error().forward("Create staging buffer failed");
			if (!device_buffer_result)
				return device_buffer_result.error().forward("Create device buffer failed");

			return StagedBuffer(std::move(*staging_buffer_result), std::move(*device_buffer_result));
		}

		///
		/// @brief Update the buffer with new data while not uploading to GPU. Call `upload` to upload the
		/// data to GPU.
		///
		/// @param data Data input
		/// @return Success, or error if update failed
		///
		[[nodiscard]]
		std::expected<void, Error> update(const T& data) noexcept
		{
			return staging_buffer.upload(data);
		}

		///
		/// @brief Upload the data to GPU. No barrier is inserted. Caller should insert the barriers as
		/// needed.
		///
		/// @param command_buffer Command buffer
		///
		void upload_direct(const vk::raii::CommandBuffer& command_buffer) const noexcept
		{
			const auto copy_region = vk::BufferCopy2{.srcOffset = 0, .dstOffset = 0, .size = sizeof(T)};

			command_buffer.copyBuffer2(
				vk::CopyBufferInfo2()
					.setSrcBuffer(staging_buffer)
					.setDstBuffer(device_buffer)
					.setRegions(copy_region)
			);
		}

		///
		/// @brief Upload the data to GPU and return a buffer memory barrier for the uploaded buffer
		///
		/// @param command_buffer Command buffer
		/// @param dst_stage_mask Destination pipeline stage mask for the buffer memory barrier
		/// @param dst_access_mask Destination access mask for the buffer memory barrier. Default is
		/// `eShaderRead`
		/// @return Buffer memory barrier
		///
		[[nodiscard]]
		vk::BufferMemoryBarrier2 upload(
			const vk::raii::CommandBuffer& command_buffer,
			vk::PipelineStageFlags2 dst_stage_mask,
			vk::AccessFlags2 dst_access_mask = vk::AccessFlagBits2::eShaderRead
		) const noexcept
		{
			upload_direct(command_buffer);

			return vk::BufferMemoryBarrier2{
				.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
				.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
				.dstStageMask = dst_stage_mask,
				.dstAccessMask = dst_access_mask,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.buffer = device_buffer,
				.offset = 0,
				.size = vk::WholeSize
			};
		}

		operator vk::Buffer() const noexcept { return device_buffer; }
		operator ElementBufferRef<T>() const noexcept { return device_buffer; }

	  private:

		ElementBuffer<T> staging_buffer, device_buffer;

		StagedBuffer(ElementBuffer<T> staging_buffer, ElementBuffer<T> device_buffer) :
			staging_buffer(std::move(staging_buffer)),
			device_buffer(std::move(device_buffer))
		{}

	  public:

		StagedBuffer(const StagedBuffer&) = delete;
		StagedBuffer(StagedBuffer&&) = default;
		StagedBuffer& operator=(const StagedBuffer&) = delete;
		StagedBuffer& operator=(StagedBuffer&&) = default;
	};

	///
	/// @brief Dynamic-length host-uploadable buffer. Holds an array of type T.
	///
	/// @tparam T Type of the buffer data
	///
	template <typename T>
	class DynArrayStagedBuffer
	{
	  public:

		///
		/// @brief Create a dynamic staged buffer with the given usage flags
		///
		/// @param usage_flags Buffer usage flags. `eTransferDst` is automatically added
		///
		DynArrayStagedBuffer(vk::BufferUsageFlags usage_flags) noexcept :
			staging_buffer(vk::BufferUsageFlagBits::eTransferSrc, vulkan::MemoryUsage::CpuToGpu),
			device_buffer(usage_flags | vk::BufferUsageFlagBits::eTransferDst, vulkan::MemoryUsage::GpuOnly)
		{}

		operator vk::Buffer() const noexcept { return device_buffer; }
		operator ArrayBufferRef<T>() const noexcept { return device_buffer; }

		///
		/// @brief Update the buffer with new data while not uploading to GPU. Call `upload` to upload the
		/// data to	GPU.
		///
		/// @param device_context Vulkan context
		/// @param data Span of the data to update
		/// @return Success, or error if update failed
		///
		[[nodiscard]]
		std::expected<void, Error> update(
			const vulkan::DeviceContext& device_context,
			std::span<const T> data
		) noexcept
		{
			if (const auto result = device_buffer.resize(device_context, data.size()); !result)
				return result.error().forward("Resize device buffer failed");

			if (const auto result = staging_buffer.resize(device_context, data.size()); !result)
				return result.error().forward("Resize staging buffer failed");

			if (data.empty()) return {};

			if (const auto result = staging_buffer->upload(std::as_bytes(data)); !result)
				return result.error().forward("Upload staging buffer failed");

			return {};
		}

		///
		/// @brief Upload the data to GPU. No barrier is inserted. Caller should insert the barriers as
		/// needed.
		///
		/// @param command_buffer Command buffer
		///
		void upload_direct(const vk::raii::CommandBuffer& command_buffer) const noexcept
		{
			const auto copy_region =
				vk::BufferCopy2{.srcOffset = 0, .dstOffset = 0, .size = staging_buffer.size_bytes()};

			if (copy_region.size > 0)
				command_buffer.copyBuffer2(
					vk::CopyBufferInfo2()
						.setSrcBuffer(staging_buffer)
						.setDstBuffer(device_buffer)
						.setRegions(copy_region)
				);
		}

		///
		/// @brief Upload the data to GPU and return a buffer memory barrier for the uploaded buffer
		///
		/// @param command_buffer Command buffer
		/// @param dst_stage_mask Destination pipeline stage mask for the buffer memory barrier
		/// @param dst_access_mask Destination access mask for the buffer memory barrier. Default is
		/// `eShaderRead`
		/// @return Buffer memory barrier
		///
		[[nodiscard]]
		vk::BufferMemoryBarrier2 upload(
			const vk::raii::CommandBuffer& command_buffer,
			vk::PipelineStageFlags2 dst_stage_mask,
			vk::AccessFlags2 dst_access_mask = vk::AccessFlagBits2::eShaderRead
		) const noexcept
		{
			upload_direct(command_buffer);

			return vk::BufferMemoryBarrier2{
				.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
				.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
				.dstStageMask = dst_stage_mask,
				.dstAccessMask = dst_access_mask,
				.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
				.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
				.buffer = device_buffer,
				.offset = 0,
				.size = device_buffer.size_bytes() == 0 ? vk::WholeSize : device_buffer.size_bytes()
			};
		}

	  private:

		vulkan::DynArrayBuffer<T> staging_buffer, device_buffer;

	  public:

		DynArrayStagedBuffer(const DynArrayStagedBuffer&) = delete;
		DynArrayStagedBuffer(DynArrayStagedBuffer&&) = default;
		DynArrayStagedBuffer& operator=(const DynArrayStagedBuffer&) = delete;
		DynArrayStagedBuffer& operator=(DynArrayStagedBuffer&&) = default;
	};
}

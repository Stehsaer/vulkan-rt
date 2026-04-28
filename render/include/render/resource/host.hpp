#pragma once

#include "render/interface/auto-exposure.hpp"
#include "render/interface/camera.hpp"
#include "render/interface/direct-light.hpp"
#include "render/interface/primitive-drawcall.hpp"
#include "render/util/per-render-state.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/util/staged-buffer.hpp"

namespace render
{
	///
	/// @brief Resources for parameters uploaded from host
	///
	class HostParamResource
	{
	  public:

		///
		/// @brief Create a host parameter resource
		///
		/// @param context Vulkan context
		/// @return Created resource or error
		///
		[[nodiscard]]
		static std::expected<HostParamResource, Error> create(const vulkan::DeviceContext& context) noexcept;

		///
		/// @brief Update the resource with new data without actually uploading to GPU
		///
		/// @param camera Camera data
		/// @param exposure_param Exposure parameters
		/// @param primary_light Primary light parameters
		/// @return `void` if success, or error
		///
		[[nodiscard]]
		std::expected<void, Error> update(
			const Camera& camera,
			const ExposureParam& exposure_param,
			const DirectLight& primary_light
		) noexcept;

		///
		/// @brief Upload the data to GPU
		///
		/// @param command_buffer Command buffer
		/// @return Buffer memory barriers for the uploaded buffers
		///
		[[nodiscard]]
		std::array<vk::BufferMemoryBarrier2, 3> upload(
			const vk::raii::CommandBuffer& command_buffer
		) const noexcept;

		struct Ref
		{
			// Camera parameter buffer
			vulkan::ElementBufferRef<Camera> camera;

			// Exposure parameter buffer
			vulkan::ElementBufferRef<ExposureParam> exposure_param;

			// Primary light parameter buffer
			vulkan::ElementBufferRef<DirectLight> primary_light;

			const Ref* operator->() const noexcept { return this; }
		};

		///
		/// @brief Get references to the buffers
		///
		/// @return References
		///
		[[nodiscard]]
		Ref ref() const noexcept
		{
			return Ref{
				.camera = camera,
				.exposure_param = exposure_param,
				.primary_light = primary_light,
			};
		}

		Ref operator->() const noexcept { return ref(); }

	  private:

		vulkan::StagedBuffer<Camera> camera;
		vulkan::StagedBuffer<ExposureParam> exposure_param;
		vulkan::StagedBuffer<DirectLight> primary_light;

		explicit HostParamResource(
			vulkan::StagedBuffer<Camera> camera,
			vulkan::StagedBuffer<ExposureParam> exposure_param,
			vulkan::StagedBuffer<DirectLight> primary_light
		) :
			camera(std::move(camera)),
			exposure_param(std::move(exposure_param)),
			primary_light(std::move(primary_light))
		{}

	  public:

		HostParamResource(const HostParamResource&) = delete;
		HostParamResource(HostParamResource&&) = default;
		HostParamResource& operator=(const HostParamResource&) = delete;
		HostParamResource& operator=(HostParamResource&&) = default;
	};

	///
	/// @brief Resources for storing drawcalls uploaded by host
	///
	class HostDrawcallResource
	{
	  public:

		HostDrawcallResource() = default;

		///
		/// @brief Update the resource with new data without actually uploading to GPU
		///
		/// @param context Vulkan context
		/// @param primitive_drawcalls Primitive drawcalls
		/// @param transforms Transforms
		/// @return `void` if success, or error
		///
		[[nodiscard]]
		std::expected<void, Error> update(
			const vulkan::DeviceContext& context,
			PerRenderState<std::span<const PrimitiveDrawcall>> primitive_drawcalls,
			std::span<const glm::mat4> transforms
		) noexcept;

		///
		/// @brief Upload to GPU
		///
		/// @param command_buffer Command buffer
		/// @return Array of memory barriers for the buffers
		///
		[[nodiscard]]
		std::array<vk::BufferMemoryBarrier2, 5> upload(
			const vk::raii::CommandBuffer& command_buffer
		) const noexcept;

		struct Ref
		{
			// Transform matrices buffer
			vulkan::ArrayBufferRef<glm::mat4> transform;

			// Primitive drawcall buffers
			PerRenderState<vulkan::ArrayBufferRef<PrimitiveDrawcall>> primitive_drawcalls;

			const Ref* operator->() const noexcept { return this; }
		};

		Ref operator->() const noexcept
		{
			return Ref{
				.transform = transform_buffer,
				.primitive_drawcalls =
					primitive_drawcall_buffers.to<vulkan::ArrayBufferRef<PrimitiveDrawcall>>(),
			};
		}

	  private:

		vulkan::DynArrayStagedBuffer<glm::mat4> transform_buffer = vulkan::DynArrayStagedBuffer<glm::mat4>(
			vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
		);

		PerRenderState<vulkan::DynArrayStagedBuffer<PrimitiveDrawcall>> primitive_drawcall_buffers =
			PerRenderState<vulkan::DynArrayStagedBuffer<PrimitiveDrawcall>>::from_args(
				vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eTransferDst
			);

	  public:

		HostDrawcallResource(const HostDrawcallResource&) = delete;
		HostDrawcallResource(HostDrawcallResource&&) = default;
		HostDrawcallResource& operator=(const HostDrawcallResource&) = delete;
		HostDrawcallResource& operator=(HostDrawcallResource&&) = default;
	};
}

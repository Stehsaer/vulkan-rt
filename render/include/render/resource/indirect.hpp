#pragma once

#include "render/interface/indirect-drawcall.hpp"
#include "render/util/per-render-state.hpp"
#include "vulkan/util/dyn-buffer.hpp"

namespace render
{
	///
	/// @brief Indirect drawcall resource
	///
	class IndirectResource
	{
	  public:

		IndirectResource() = default;

		///
		/// @brief Resize the resource buffer
		///
		/// @param context Vulkan context
		/// @param drawcall_counts Element count of drawcalls
		/// @return `void` if success, or error
		///
		[[nodiscard]]
		std::expected<void, Error> resize(
			const vulkan::DeviceContext& context,
			render::PerRenderState<size_t> drawcall_counts
		) noexcept;

		///
		/// @brief Get references to the buffer
		///
		/// @return References to the buffer
		///
		[[nodiscard]]
		PerRenderState<vulkan::ArrayBufferRef<IndirectDrawcall>> ref() const noexcept
		{
			return indirect_drawcall_buffers.to<vulkan::ArrayBufferRef<IndirectDrawcall>>();
		}

		operator PerRenderState<vulkan::ArrayBufferRef<IndirectDrawcall>>() const noexcept { return ref(); }

	  private:

		PerRenderState<vulkan::DynArrayBuffer<IndirectDrawcall>> indirect_drawcall_buffers =
			PerRenderState<vulkan::DynArrayBuffer<IndirectDrawcall>>::from_args(
				vk::BufferUsageFlagBits::eIndirectBuffer | vk::BufferUsageFlagBits::eStorageBuffer,
				vulkan::MemoryUsage::GpuOnly
			);

	  public:

		IndirectResource(const IndirectResource&) = delete;
		IndirectResource(IndirectResource&&) = default;
		IndirectResource& operator=(const IndirectResource&) = delete;
		IndirectResource& operator=(IndirectResource&&) = default;
	};
}

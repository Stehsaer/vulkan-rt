#include "render/resource/forward-rendering.hpp"
#include "common/util/error.hpp"
#include "vulkan/container/device/frame-buffer.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <utility>

namespace render
{
	std::expected<void, Error> ForwardRenderResource::resize(
		const vulkan::Context& context,
		glm::u32vec2 extent
	) noexcept
	{
		depth.reset();
		hdr.reset();

		auto depth_result =
			vulkan::FrameBuffer::create(context.device, context.allocator, extent, DEPTH_FORMAT);
		if (!depth_result) return depth_result.error().forward("Create depth buffer failed");
		depth = std::move(*depth_result);

		auto hdr_result = vulkan::FrameBuffer::create(context.device, context.allocator, extent, HDR_FORMAT);
		if (!hdr_result) return hdr_result.error().forward("Create HDR buffer failed");
		hdr = std::move(*hdr_result);

		return {};
	}
}

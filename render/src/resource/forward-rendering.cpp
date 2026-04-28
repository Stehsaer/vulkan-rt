#include "render/resource/forward-rendering.hpp"
#include "vulkan/util/frame-buffer.hpp"

namespace render
{
	bool ForwardRenderResource::is_complete() const noexcept
	{
		return depth.has_value() && hdr.has_value();
	}

	std::expected<void, Error> ForwardRenderResource::resize(
		const vulkan::DeviceContext& context,
		glm::u32vec2 extent
	) noexcept
	{
		depth.reset();
		hdr.reset();

		auto depth_result =
			vulkan::FrameBuffer::create_depth(context.device, context.allocator, extent, DEPTH_FORMAT);
		if (!depth_result) return depth_result.error().forward("Create depth buffer failed");
		depth = std::move(*depth_result);

		auto hdr_result =
			vulkan::FrameBuffer::create_color(context.device, context.allocator, extent, HDR_FORMAT);
		if (!hdr_result) return hdr_result.error().forward("Create HDR buffer failed");
		hdr = std::move(*hdr_result);

		return {};
	}
}

#include "render/resource/forward.hpp"
#include "common/util/error.hpp"
#include "vulkan/container/device/attachment.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <utility>

namespace render
{
	std::expected<ForwardAttachments, Error> ForwardAttachments::create(
		const vulkan::Context& context,
		glm::u32vec2 extent
	) noexcept
	{
		auto depth_result =
			vulkan::Attachment::create(context.device, context.allocator, extent, DEPTH_FORMAT);
		if (!depth_result) return depth_result.error().forward("Create depth attachment failed");
		auto depth = std::move(*depth_result);

		auto hdr_result = vulkan::Attachment::create(context.device, context.allocator, extent, HDR_FORMAT);
		if (!hdr_result) return hdr_result.error().forward("Create HDR attachment failed");
		auto hdr = std::move(*hdr_result);

		return ForwardAttachments(extent, std::move(depth), std::move(hdr));
	}
}

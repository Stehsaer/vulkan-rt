#include "render/resource/hdr.hpp"
#include "common/util/error.hpp"
#include "vulkan/container/device/attachment.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <utility>

namespace render
{
	std::expected<HdrAttachment, Error> HdrAttachment::create(
		const vulkan::Context& context,
		glm::u32vec2 extent
	) noexcept
	{
		auto albedo_result =
			vulkan::Attachment::create(context.device, context.allocator, extent, HDR_FORMAT);
		if (!albedo_result) return albedo_result.error().forward("Create albedo buffer failed");

		return HdrAttachment(extent, std::move(*albedo_result));
	}
}

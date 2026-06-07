#include "render/resource/deferred.hpp"
#include "common/util/error.hpp"
#include "vulkan/container/device/attachment.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <utility>

namespace render
{
	std::expected<DeferredAttachment, Error> DeferredAttachment::create(
		const vulkan::Context& context,
		glm::u32vec2 extent
	) noexcept
	{
		auto albedo_result =
			vulkan::Attachment::create(context.device, context.allocator, extent, ALBEDO_FORMAT);
		if (!albedo_result) return albedo_result.error().forward("Create depth buffer failed");

		auto normal_result =
			vulkan::Attachment::create(context.device, context.allocator, extent, NORMAL_FORMAT);
		if (!normal_result) return normal_result.error().forward("Create depth buffer failed");

		auto pbr_result = vulkan::Attachment::create(context.device, context.allocator, extent, PBR_FORMAT);
		if (!pbr_result) return pbr_result.error().forward("Create depth buffer failed");

		auto depth_result =
			vulkan::Attachment::create(context.device, context.allocator, extent, DEPTH_FORMAT);
		if (!depth_result) return depth_result.error().forward("Create depth buffer failed");

		return DeferredAttachment(
			extent,
			std::move(*albedo_result),
			std::move(*normal_result),
			std::move(*pbr_result),
			std::move(*depth_result)
		);
	}
}

#include "render/resource/shadow.hpp"
#include "common/number-literals.hpp"
#include "common/util/error.hpp"
#include "vulkan/container/device/attachment.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <utility>
#include <vulkan/vulkan.hpp>

namespace render
{
	std::expected<ShadowAttachment, Error> ShadowAttachment::create(
		const vulkan::Context& context,
		glm::u32vec2 full_extent
	) noexcept
	{
		const auto half_extent = (full_extent + 1_u32) / 2_u32;

		auto shadow_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			SHADOW_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!shadow_result) return shadow_result.error().forward("Create shadow attachment failed");

		return ShadowAttachment(half_extent, full_extent, std::move(*shadow_result));
	}
}

#include "render/resource/motion-vector.hpp"
#include "common/number-literals.hpp"
#include "common/util/error.hpp"
#include "vulkan/container/device/attachment.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	std::expected<MotionVectorAttachment, Error> MotionVectorAttachment::create(
		const vulkan::Context& context,
		const vk::raii::CommandBuffer& command_buffer,
		glm::u32vec2 full_extent
	) noexcept
	{
		const auto half_extent = (full_extent + 1_u32) / 2_u32;

		auto motion_vector_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			MOTION_VECTOR_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!motion_vector_result)
			return motion_vector_result.error().forward("Create motion vector attachment failed");

		motion_vector_result->clear_color_float(command_buffer);

		return MotionVectorAttachment(half_extent, full_extent, std::move(*motion_vector_result));
	}
}

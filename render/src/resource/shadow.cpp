#include "render/resource/shadow.hpp"
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
	std::expected<ShadowAttachment, Error> ShadowAttachment::create(
		const vulkan::Context& context,
		const vk::raii::CommandBuffer& command_buffer,
		glm::u32vec2 full_extent
	) noexcept
	{
		const auto half_extent = (full_extent + 1_u32) / 2_u32;

		auto init_sample_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			SHADOW_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!init_sample_result)
			return init_sample_result.error().forward("Create shadow attachment (init-sample) failed");

		auto denoise_imm_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			SHADOW_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!denoise_imm_result)
			return denoise_imm_result.error().forward("Create shadow attachment (imm) failed");

		auto denoise_final_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			SHADOW_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!denoise_final_result)
			return denoise_final_result.error().forward("Create shadow attachment (final) failed");

		init_sample_result->clear_color_float(command_buffer);

		return ShadowAttachment(
			half_extent,
			full_extent,
			std::move(*init_sample_result),
			std::move(*denoise_imm_result),
			std::move(*denoise_final_result)
		);
	}
}

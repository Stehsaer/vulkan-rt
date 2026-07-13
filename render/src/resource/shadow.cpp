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
			INTERMEDIATE_TEX_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!init_sample_result)
			return init_sample_result.error().forward("Create init_sample texture failed");

		auto spatial_mean_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			INTERMEDIATE_TEX_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!spatial_mean_result)
			return spatial_mean_result.error().forward("Create spatial_mean texture failed");

		auto spatial_stddev_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			INTERMEDIATE_TEX_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!spatial_stddev_result)
			return spatial_stddev_result.error().forward("Create spatial_stddev texture failed");

		auto filtered_spatial_stddev_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			INTERMEDIATE_TEX_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!filtered_spatial_stddev_result)
			return filtered_spatial_stddev_result.error().forward("Create spatial_stddev texture failed");

		auto history_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			VISIBILITY_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!history_result) return history_result.error().forward("Create history texture failed");

		auto denoise_alice_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			DENOISE_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!denoise_alice_result)
			return denoise_alice_result.error().forward("Create denoise_alice texture failed");

		auto denoise_bob_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			DENOISE_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!denoise_bob_result)
			return denoise_bob_result.error().forward("Create denoise_bob texture failed");

		auto visibility_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			full_extent,
			VISIBILITY_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!visibility_result) return visibility_result.error().forward("Create visibility texture failed");

		history_result->clear_color_float(command_buffer);

		return ShadowAttachment(
			half_extent,
			full_extent,
			std::move(*init_sample_result),
			std::move(*spatial_mean_result),
			std::move(*spatial_stddev_result),
			std::move(*filtered_spatial_stddev_result),
			std::move(*history_result),
			std::move(*denoise_alice_result),
			std::move(*denoise_bob_result),
			std::move(*visibility_result)
		);
	}
}

#include "render/resource/deferred.hpp"
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
	std::expected<DeferredAttachment, Error> DeferredAttachment::create(
		const vulkan::Context& context,
		const vk::raii::CommandBuffer& command_buffer,
		glm::u32vec2 extent
	) noexcept
	{
		auto albedo_result =
			vulkan::Attachment::create(context.device, context.allocator, extent, ALBEDO_FORMAT);
		if (!albedo_result) return albedo_result.error().forward("Create albedo buffer failed");

		auto normal_result =
			vulkan::Attachment::create(context.device, context.allocator, extent, NORMAL_FORMAT);
		if (!normal_result) return normal_result.error().forward("Create normal buffer failed");

		auto pbr_result = vulkan::Attachment::create(context.device, context.allocator, extent, PBR_FORMAT);
		if (!pbr_result) return pbr_result.error().forward("Create pbr buffer failed");

		auto depth_result =
			vulkan::Attachment::create(context.device, context.allocator, extent, DEPTH_FORMAT);
		if (!depth_result) return depth_result.error().forward("Create depth buffer failed");

		albedo_result->clear_color_float(command_buffer);
		normal_result->clear_color_float(command_buffer);
		pbr_result->clear_color_float(command_buffer);
		depth_result->clear_depth_stencil(command_buffer);

		return DeferredAttachment(
			extent,
			std::move(*albedo_result),
			std::move(*normal_result),
			std::move(*pbr_result),
			std::move(*depth_result)
		);
	}

	std::expected<HalfDeferredAttachment, Error> HalfDeferredAttachment::create(
		const vulkan::Context& context,
		const vk::raii::CommandBuffer& command_buffer,
		glm::u32vec2 extent
	) noexcept
	{
		const auto half_extent = (extent + 1_u32) / 2_u32;

		auto half_albedo_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			HALF_ALBEDO_STORAGE_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!half_albedo_result)
			return half_albedo_result.error().forward("Create half-res albedo buffer failed");

		auto half_normal_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			HALF_NORMAL_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!half_normal_result)
			return half_normal_result.error().forward("Create half-res normal buffer failed");

		auto half_pbr_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			HALF_PBR_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!half_pbr_result) return half_pbr_result.error().forward("Create half-res pbr buffer failed");

		auto half_depth_result = vulkan::Attachment::create(
			context.device,
			context.allocator,
			half_extent,
			HALF_DEPTH_FORMAT,
			vk::ImageUsageFlagBits::eStorage
		);
		if (!half_depth_result)
			return half_depth_result.error().forward("Create half-res depth buffer failed");

		half_albedo_result->clear_color_float(command_buffer);
		half_normal_result->clear_color_float(command_buffer);
		half_pbr_result->clear_color_float(command_buffer);
		half_depth_result->clear_color_float(command_buffer);

		return HalfDeferredAttachment(
			extent,
			half_extent,
			std::move(*half_albedo_result),
			std::move(*half_normal_result),
			std::move(*half_pbr_result),
			std::move(*half_depth_result)
		);
	}
}

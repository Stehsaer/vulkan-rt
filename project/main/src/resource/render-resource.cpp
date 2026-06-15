#include "resource/render-resource.hpp"
#include "common/util/array.hpp"
#include "common/util/error.hpp"
#include "render/interface/primitive-drawcall.hpp"
#include "render/resource/auto-exposure.hpp"
#include "render/resource/deferred.hpp"
#include "render/resource/hdr.hpp"
#include "render/resource/host.hpp"
#include "render/resource/motion-vector.hpp"
#include "render/resource/shadow.hpp"
#include "render/util/per-render-state.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <glm/ext/vector_uint2_sized.hpp>
#include <span>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace resource
{
	std::expected<RenderResource, Error> RenderResource::create(const vulkan::Context& context) noexcept
	{
		auto param_result = render::HostParamResource::create(context);
		if (!param_result) return param_result.error().forward("Create host param resource failed");

		auto auto_exposure_result = render::AutoExposureResource::create(context);
		if (!auto_exposure_result)
			return auto_exposure_result.error().forward("Create auto exposure resource failed");

		return RenderResource{
			.param = std::move(*param_result),
			.drawcall = {},
			.indirect = {},
			.auto_exposure = std::move(*auto_exposure_result)
		};
	}

	std::expected<void, Error> RenderResource::update(
		const vulkan::Context& context,
		const RenderData& data
	) noexcept
	{
		if (const auto result = param.update(data.camera, data.exposure_param, data.primary_light); !result)
			return result.error().forward("Update host param resource failed");

		if (const auto result = drawcall.update(context, data.drawcalls, data.transforms); !result)
			return result.error().forward("Update host drawcall resource failed");

		if (const auto result =
				indirect
					.resize(context, data.drawcalls.map(&std::span<const render::PrimitiveDrawcall>::size));
			!result)
			return result.error().forward("Update indirect resource failed");

		return {};
	}

	std::expected<void, Error> RenderResource::resize_attachments(
		const vulkan::Context& context,
		const vk::raii::CommandBuffer& command_buffer,
		glm::u32vec2 extent
	) noexcept
	{
		auto deferred_result = render::DeferredAttachment::create(context, command_buffer, extent);
		if (!deferred_result) return deferred_result.error().forward("Create deferred attachments failed");

		auto half_deferred_result = render::HalfDeferredAttachment::create(context, command_buffer, extent);
		if (!half_deferred_result)
			return half_deferred_result.error().forward("Create half-res deferred attachments failed");

		auto shadow_result = render::ShadowAttachment::create(context, command_buffer, extent);
		if (!shadow_result) return shadow_result.error().forward("Create shadow attachment failed");

		auto motion_vector_result = render::MotionVectorAttachment::create(context, command_buffer, extent);
		if (!motion_vector_result)
			return motion_vector_result.error().forward("Create motion vector attachment failed");

		auto hdr_result = render::HdrAttachment::create(context, command_buffer, extent);
		if (!hdr_result) return hdr_result.error().forward("Create HDR attachments failed");

		attachments = Attachments{
			.deferred = std::move(*deferred_result),
			.half_deferred = std::move(*half_deferred_result),
			.shadow = std::move(*shadow_result),
			.motion_vector = std::move(*motion_vector_result),
			.hdr = std::move(*hdr_result),
		};

		return {};
	}

	void RenderResource::upload(const vk::raii::CommandBuffer& command_buffer) const noexcept
	{
		const auto host_param_barriers = param.upload(command_buffer);
		const auto host_drawcall_barriers = drawcall.upload(command_buffer);

		const auto barriers = util::array_concat(host_param_barriers, host_drawcall_barriers);

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setBufferMemoryBarriers(barriers));
	}
}

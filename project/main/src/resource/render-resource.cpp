#include "resource/render-resource.hpp"
#include "common/util/array.hpp"
#include "common/util/error.hpp"
#include "render/interface/primitive-drawcall.hpp"
#include "render/resource/auto-exposure.hpp"
#include "render/resource/forward.hpp"
#include "render/resource/host.hpp"
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
		glm::u32vec2 extent
	) noexcept
	{
		auto forward_result = render::ForwardAttachments::create(context, extent);
		if (!forward_result) return forward_result.error().forward("Create forward attachments failed");

		attachments = Attachments{.forward = std::move(*forward_result)};
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

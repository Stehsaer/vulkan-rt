#include "resource/render-resource.hpp"
#include "common/util/array.hpp"
#include "render/resource/host.hpp"
#include "render/util/per-render-state.hpp"

#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_enums.hpp>

namespace resource
{
	std::expected<RenderResource, Error> RenderResource::create(const vulkan::DeviceContext& context) noexcept
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
			.forward = {},
			.auto_exposure = std::move(*auto_exposure_result)
		};
	}

	std::expected<void, Error> RenderResource::update(
		const vulkan::DeviceContext& context,
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

	std::expected<void, Error> RenderResource::resize(
		const vulkan::DeviceContext& context,
		glm::u32vec2 extent
	) noexcept
	{
		if (const auto result = forward.resize(context, extent); !result)
			return result.error().forward("Resize forward render resource failed");

		return {};
	}

	[[nodiscard]]
	bool RenderResource::is_complete() const noexcept
	{
		return forward.is_complete();
	}

	void RenderResource::upload(const vk::raii::CommandBuffer& command_buffer) const noexcept
	{
		const auto host_param_barriers = param.upload(command_buffer);
		const auto host_drawcall_barriers = drawcall.upload(command_buffer);

		const auto barriers = util::array_concat(host_param_barriers, host_drawcall_barriers);

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setBufferMemoryBarriers(barriers));
	}
}

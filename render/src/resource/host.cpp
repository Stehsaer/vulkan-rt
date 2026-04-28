#include "render/resource/host.hpp"
#include "common/util/array.hpp"

namespace render
{
	std::expected<HostParamResource, Error> HostParamResource::create(
		const vulkan::DeviceContext& context
	) noexcept
	{
		auto camera_buffer_result =
			vulkan::StagedBuffer<Camera>::create(context, vk::BufferUsageFlagBits::eUniformBuffer);
		if (!camera_buffer_result) return camera_buffer_result.error().forward("Create camera buffer failed");

		auto exposure_param_buffer_result =
			vulkan::StagedBuffer<ExposureParam>::create(context, vk::BufferUsageFlagBits::eUniformBuffer);
		if (!exposure_param_buffer_result)
			return exposure_param_buffer_result.error().forward("Create exposure param buffer failed");

		auto primary_light_buffer_result =
			vulkan::StagedBuffer<DirectLight>::create(context, vk::BufferUsageFlagBits::eUniformBuffer);
		if (!primary_light_buffer_result)
			return primary_light_buffer_result.error().forward("Create main light buffer failed");

		return HostParamResource(
			std::move(*camera_buffer_result),
			std::move(*exposure_param_buffer_result),
			std::move(*primary_light_buffer_result)
		);
	}

	std::expected<void, Error> HostParamResource::update(
		const Camera& camera,
		const ExposureParam& exposure_param,
		const DirectLight& primary_light
	) noexcept
	{
		auto camera_update_result = this->camera.update(camera);
		if (!camera_update_result) return camera_update_result.error().forward("Update camera buffer failed");

		auto exposure_param_update_result = this->exposure_param.update(exposure_param);
		if (!exposure_param_update_result)
			return exposure_param_update_result.error().forward("Update exposure param buffer failed");

		auto primary_light_update_result = this->primary_light.update(primary_light);
		if (!primary_light_update_result)
			return primary_light_update_result.error().forward("Update main light buffer failed");

		return {};
	}

	std::array<vk::BufferMemoryBarrier2, 3> HostParamResource::upload(
		const vk::raii::CommandBuffer& command_buffer
	) const noexcept
	{
		// TODO: Raytrace pipeline stage flag bits

		const auto camera_barrier = camera.upload(
			command_buffer,
			vk::PipelineStageFlagBits2::eAllGraphics | vk::PipelineStageFlagBits2::eComputeShader
		);

		const auto exposure_param_barrier = exposure_param.upload(
			command_buffer,
			vk::PipelineStageFlagBits2::eAllGraphics | vk::PipelineStageFlagBits2::eComputeShader
		);

		const auto primary_light_barrier = primary_light.upload(
			command_buffer,
			vk::PipelineStageFlagBits2::eAllGraphics | vk::PipelineStageFlagBits2::eComputeShader
		);

		return std::to_array({
			camera_barrier,
			exposure_param_barrier,
			primary_light_barrier,
		});
	}

	std::expected<void, Error> HostDrawcallResource::update(
		const vulkan::DeviceContext& context,
		PerRenderState<std::span<const PrimitiveDrawcall>> primitive_drawcalls,
		std::span<const glm::mat4> transforms
	) noexcept
	{
		if (const auto result = transform_buffer.update(context, transforms); !result)
			return result.error().forward("Update transform buffer failed");

		for (const auto& [data, buffer] :
			 std::views::zip(primitive_drawcalls.all(), primitive_drawcall_buffers.all()))
		{
			if (const auto result = buffer.update(context, data); !result)
				return result.error().forward("Update primitive drawcall buffer failed");
		}

		return {};
	}

	std::array<vk::BufferMemoryBarrier2, 5> HostDrawcallResource::upload(
		const vk::raii::CommandBuffer& command_buffer
	) const noexcept
	{
		const auto transform_barrier = transform_buffer.upload(
			command_buffer,
			vk::PipelineStageFlagBits2::eVertexShader | vk::PipelineStageFlagBits2::eComputeShader
		);

		const auto primitive_drawcall_barriers =
			primitive_drawcall_buffers
				.map([&](const auto& buffer) {
					return buffer.upload(command_buffer, vk::PipelineStageFlagBits2::eComputeShader);
				})
				.as_array();

		return util::array_concat(transform_barrier, primitive_drawcall_barriers);
	}
}

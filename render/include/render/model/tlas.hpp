#pragma once

#include "common/util/error.hpp"
#include "render/model/model.hpp"
#include "vulkan/alloc/buffer.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <glm/ext/matrix_float4x4.hpp>
#include <span>
#include <utility>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	class Tlas
	{
	  public:

		static std::expected<Tlas, Error> build(
			const vulkan::Context& context,
			const Model& model,
			std::span<const glm::mat4> transforms
		) noexcept;

		// TODO: update TLAS

	  private:

		vulkan::ArrayBuffer<vk::AccelerationStructureInstanceKHR> instance_buffer;
		vulkan::Buffer tlas_buffer;
		vk::raii::AccelerationStructureKHR tlas;

		explicit Tlas(
			vulkan::ArrayBuffer<vk::AccelerationStructureInstanceKHR> instance_buffer,
			vulkan::Buffer tlas_buffer,
			vk::raii::AccelerationStructureKHR tlas
		) :
			instance_buffer(std::move(instance_buffer)),
			tlas_buffer(std::move(tlas_buffer)),
			tlas(std::move(tlas))
		{}

	  public:

		Tlas(const Tlas&) = delete;
		Tlas(Tlas&&) = default;
		Tlas& operator=(const Tlas&) = delete;
		Tlas& operator=(Tlas&&) = default;
	};
}

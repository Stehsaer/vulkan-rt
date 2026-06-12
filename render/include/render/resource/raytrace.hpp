#pragma once

#include "common/util/error.hpp"
#include "render/model/model.hpp"
#include "vulkan/interface/context.hpp"

#include <expected>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	///
	/// @brief Raytracing resource set layout
	/// @note Primarily provides mesh data to the raytracing pipelines
	///
	class RaytraceResourceLayout
	{
	  public:

		///
		/// @brief Create raytracing resource layout
		///
		/// @param context Vulkan context
		/// @return Created layout or error
		///
		[[nodiscard]]
		static std::expected<RaytraceResourceLayout, Error> create(const vulkan::Context& context) noexcept;

		struct View
		{
			vk::DescriptorSetLayout mesh_resource;

			auto operator->() const noexcept { return this; }
		};

		operator View() const noexcept { return View{.mesh_resource = mesh_resource_layout}; }
		View operator->() const noexcept { return *this; }

	  private:

		vk::raii::DescriptorSetLayout mesh_resource_layout;

		explicit RaytraceResourceLayout(vk::raii::DescriptorSetLayout mesh_resource_layout) :
			mesh_resource_layout(std::move(mesh_resource_layout))
		{}

	  public:

		RaytraceResourceLayout(const RaytraceResourceLayout&) = delete;
		RaytraceResourceLayout(RaytraceResourceLayout&&) = default;
		RaytraceResourceLayout& operator=(const RaytraceResourceLayout&) = delete;
		RaytraceResourceLayout& operator=(RaytraceResourceLayout&&) = default;
	};

	///
	/// @brief Raytracing resource
	/// @note Primarily provides mesh data to the raytracing pipelines
	///
	struct RaytraceResource
	{
	  public:

		///
		/// @brief Create raytracing resource from model
		///
		/// @param context Vulkan context
		/// @param layout Resource layout
		/// @param model Model instance
		/// @return Created resource or error
		///
		[[nodiscard]]
		static std::expected<RaytraceResource, Error> create(
			const vulkan::Context& context,
			const RaytraceResourceLayout& layout,
			const Model& model
		) noexcept;

		struct View
		{
			vk::DescriptorSet mesh_resource;

			auto operator->() const noexcept { return this; }
		};

		operator View() const noexcept { return View{.mesh_resource = mesh_resource_set}; }
		View operator->() const noexcept { return *this; }

	  private:

		vk::raii::DescriptorPool pool;
		vk::raii::DescriptorSet mesh_resource_set;

		explicit RaytraceResource(vk::raii::DescriptorPool pool, vk::raii::DescriptorSet mesh_resource_set) :
			pool(std::move(pool)),
			mesh_resource_set(std::move(mesh_resource_set))
		{}

	  public:

		RaytraceResource(const RaytraceResource&) = delete;
		RaytraceResource(RaytraceResource&&) = default;
		RaytraceResource& operator=(const RaytraceResource&) = delete;
		RaytraceResource& operator=(RaytraceResource&&) = default;
	};
}

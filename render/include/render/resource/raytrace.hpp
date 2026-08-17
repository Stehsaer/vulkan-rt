#pragma once

#include "common/util/error.hpp"
#include "render/model/model.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/util/trivial-descriptor-set.hpp"

#include <expected>
#include <tuple>
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

		struct MeshInput : public vulkan::trivset::LayoutBase
		{
			StorageBuffer primitive_attr;
			StorageBuffer vertex_buffer;
			StorageBuffer index_buffer;

			static constexpr auto STAGE =
				vk::ShaderStageFlagBits::eAnyHitKHR | vk::ShaderStageFlagBits::eClosestHitKHR;
			static constexpr auto SLOT_LIST = std::make_tuple(
				&MeshInput::primitive_attr,
				&MeshInput::vertex_buffer,
				&MeshInput::index_buffer
			);
		};

		///
		/// @brief Create raytracing resource layout
		///
		/// @param context Vulkan context
		/// @return Created layout or error
		///
		[[nodiscard]]
		static std::expected<RaytraceResourceLayout, Error> create(const vulkan::Context& context) noexcept;

		///
		/// @brief Get the underlying descriptor set layout
		///
		/// @return Underlying descriptor set layout
		///
		[[nodiscard]]
		const vulkan::trivset::Layout<MeshInput>& get_layout() const noexcept
		{
			return mesh_resource_layout;
		}

		struct View
		{
			vk::DescriptorSetLayout mesh_resource;

			auto operator->() const noexcept { return this; }
		};

		operator View() const noexcept { return View{.mesh_resource = mesh_resource_layout}; }
		View operator->() const noexcept { return *this; }

	  private:

		vulkan::trivset::Layout<MeshInput> mesh_resource_layout;

		explicit RaytraceResourceLayout(vulkan::trivset::Layout<MeshInput> mesh_resource_layout) :
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

		vulkan::trivset::Set<RaytraceResourceLayout::MeshInput> mesh_resource_set;

		explicit RaytraceResource(vulkan::trivset::Set<RaytraceResourceLayout::MeshInput> mesh_resource_set) :
			mesh_resource_set(std::move(mesh_resource_set))
		{}

	  public:

		RaytraceResource(const RaytraceResource&) = delete;
		RaytraceResource(RaytraceResource&&) = default;
		RaytraceResource& operator=(const RaytraceResource&) = delete;
		RaytraceResource& operator=(RaytraceResource&&) = default;
	};
}

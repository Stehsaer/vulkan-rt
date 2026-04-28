#pragma once

#include "common/util/error.hpp"
#include "common/util/tagged-type.hpp"
#include "model/material.hpp"
#include "render/model/texture-list.hpp"
#include "vulkan/alloc/buffer.hpp"
#include "vulkan/interface/common-context.hpp"

#include <coro/coro.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace render
{
	///
	/// @brief Maximum texture count
	///
	///
	constexpr uint32_t MAX_TEXTURE_COUNT = 65536;

	///
	/// @brief Descriptor set layout for materials
	/// @note See *render/model/doc/material-layout.md*
	///
	struct MaterialLayout
	{
		vk::raii::DescriptorSetLayout layout;

		///
		/// @brief Create descriptor set layout for materials
		///
		/// @param device Vulkan device to create the layout with
		/// @return Created `MaterialLayout` if successful, or an `Error` if creation fails
		///
		[[nodiscard]]
		static std::expected<MaterialLayout, Error> create(const vk::raii::Device& device) noexcept;
	};

	///
	/// @brief Index into the array of textures
	///
	///
	struct TextureIndex
	{
		uint32_t albedo, emissive, orm, normal;
	};

	///
	/// @brief Material info, stores contiguously in the info buffer
	///
	///
	struct MaterialInfo
	{
		TextureIndex texture_index;
		model::Material::Param param;
	};

	///
	/// @brief Material list, manages all material-related GPU resources, including textures, samplers, info
	/// buffer, and descriptor sets
	///
	///
	class MaterialList
	{
	  public:

		///
		/// @brief State of progress when creating material list
		///
		///
		enum class ProgressState
		{
			Preparing,
			TextureList,
			Processing
		};

		///
		/// @brief Progress of creating material list
		///
		///
		using Progress = util::SyncedEnumVariant<
			util::Tag<ProgressState::Preparing>,
			util::Tag<ProgressState::TextureList, util::Progress::Ref>,
			util::Tag<ProgressState::Processing>
		>;

		///
		/// @brief Create a material list from CPU-side material list
		///
		/// @param thread_pool Coroutine thread pool
		/// @param progress Progress object to report progress during creation
		/// @param context Vulkan context
		/// @param layout Descriptor set layout for materials
		/// @param material_list CPU-side material list
		/// @param texture_load_option Options for loading textures
		/// @return Created `MaterialList` if successful, or an `Error` if fails
		///
		[[nodiscard]]
		static std::pair<coro::task<std::expected<MaterialList, Error>>, std::shared_ptr<const Progress>>
		create(
			coro::thread_pool& thread_pool,
			const vulkan::DeviceContext& context,
			const MaterialLayout& layout,
			const model::MaterialList& material_list,
			TextureList::LoadOption texture_load_option
		) noexcept;

		///
		/// @brief Get the descriptor set object for the material list
		///
		/// @return Descriptor set object
		///
		[[nodiscard]]
		vk::DescriptorSet get_descriptor_set() const noexcept
		{
			return descriptor_set;
		}

		///
		/// @brief Query material mode by material index
		///
		/// @param material_index Index of the material to query, for default material, use `0xFFFFFFFF`
		/// @return Material mode for the specified material index
		///
		[[nodiscard]]
		model::Material::Mode query_material_mode(uint32_t material_index) const noexcept
		{
			return material_modes[material_index == 0xFFFFFFFF ? 0 : material_index + 1];
		}

	  private:

		TextureList texture_list;

		std::vector<vk::raii::ImageView> textures;
		std::vector<vk::raii::Sampler> samplers;
		vulkan::ArrayBuffer<MaterialInfo> info_buffer;
		std::vector<model::Material::Mode> material_modes;

		vk::raii::DescriptorPool pool;
		vk::raii::DescriptorSet descriptor_set;

		explicit MaterialList(
			TextureList texture_list,
			std::vector<vk::raii::ImageView> textures,
			std::vector<vk::raii::Sampler> samplers,
			vulkan::ArrayBuffer<MaterialInfo> info_buffer,
			std::vector<model::Material::Mode> material_modes,
			vk::raii::DescriptorPool pool,
			vk::raii::DescriptorSet descriptor_set
		) :
			texture_list(std::move(texture_list)),
			textures(std::move(textures)),
			samplers(std::move(samplers)),
			info_buffer(std::move(info_buffer)),
			material_modes(std::move(material_modes)),
			pool(std::move(pool)),
			descriptor_set(std::move(descriptor_set))
		{}

		[[nodiscard]]
		static coro::task<std::expected<MaterialList, Error>> create_impl(
			coro::thread_pool& thread_pool,
			std::shared_ptr<Progress> progress,
			const vulkan::DeviceContext& context,
			const MaterialLayout& layout,
			const model::MaterialList& material_list,
			TextureList::LoadOption texture_load_option
		) noexcept;

	  public:

		MaterialList(const MaterialList&) = delete;
		MaterialList(MaterialList&&) = default;
		MaterialList& operator=(const MaterialList&) = delete;
		MaterialList& operator=(MaterialList&&) = default;
	};
}

#include "render/model/material.hpp"
#include "model/material-collector.hpp"
#include "model/material.hpp"
#include "vulkan/alloc/buffer-ref.hpp"
#include "vulkan/interface/common-context.hpp"
#include "vulkan/util/static-resource-creator.hpp"

#include <expected>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace render
{
	std::expected<MaterialLayout, Error> MaterialLayout::create(const vk::raii::Device& device) noexcept
	{
		constexpr auto binding0_param_array = vk::DescriptorSetLayoutBinding{
			.binding = 0,
			.descriptorType = vk::DescriptorType::eStorageBuffer,
			.descriptorCount = 1,
			.stageFlags = VULKAN_HPP_NAMESPACE::ShaderStageFlagBits::eAll
		};

		constexpr auto binding1_texture_dynamic_array = vk::DescriptorSetLayoutBinding{
			.binding = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler,
			.descriptorCount = MAX_TEXTURE_COUNT,
			.stageFlags = VULKAN_HPP_NAMESPACE::ShaderStageFlagBits::eAll
		};

		constexpr auto bindings = std::to_array<vk::DescriptorSetLayoutBinding>({
			binding0_param_array,
			binding1_texture_dynamic_array,
		});

		const auto binding_flags = std::to_array<vk::DescriptorBindingFlags>({
			{},
			vk::DescriptorBindingFlagBits::ePartiallyBound
				| vk::DescriptorBindingFlagBits::eVariableDescriptorCount,
		});
		const auto binding_flags_create_info =
			vk::DescriptorSetLayoutBindingFlagsCreateInfo().setBindingFlags(binding_flags);

		const auto layout_create_info =
			vk::DescriptorSetLayoutCreateInfo().setPNext(&binding_flags_create_info).setBindings(bindings);
		auto descriptor_set_layout_result =
			device.createDescriptorSetLayout(layout_create_info).transform_error(Error::from<vk::Result>());
		if (!descriptor_set_layout_result)
			return descriptor_set_layout_result.error().forward("Create descriptor set layout failed");

		return MaterialLayout{.layout = std::move(*descriptor_set_layout_result)};
	}

	// Steps and helpers for creating material list
	namespace
	{
		std::expected<vulkan::ArrayBuffer<MaterialInfo>, Error> create_info_buffer(
			const vulkan::DeviceContext& context,
			const std::span<const MaterialInfo> material_infos
		) noexcept
		{
			auto resource_creator = vulkan::StaticResourceCreator(context);

			auto buffer_result =
				resource_creator.create_array_buffer(material_infos, vk::BufferUsageFlagBits::eStorageBuffer);
			if (!buffer_result) return buffer_result.error().forward("Create info buffer failed");

			if (const auto upload_result = resource_creator.execute_uploads(); !upload_result)
				return upload_result.error().forward("Upload info buffer failed");

			return buffer_result;
		}

		std::expected<
			std::tuple<
				std::vector<vk::raii::ImageView>,
				std::vector<vk::raii::Sampler>,
				std::vector<std::pair<vk::ImageView, vk::Sampler>>,
				std::vector<MaterialInfo>,
				std::vector<model::Material::Mode>
			>,
			Error
		>
		collect_textures(
			const vulkan::DeviceContext& context,
			const TextureList& texture_list,
			const model::MaterialList& material_list
		) noexcept
		{
			impl::MaterialCollector texture_collector;
			std::vector<MaterialInfo> material_infos;
			std::vector<model::Material::Mode> material_modes;

			// Collect fallback texture
			const auto fallback_texture_index_result =
				texture_collector.add_material(context.device, texture_list, model::TextureSet{});
			if (!fallback_texture_index_result)
				return fallback_texture_index_result.error().forward("Collect fallback texture failed");
			material_infos.emplace_back(
				MaterialInfo{.texture_index = *fallback_texture_index_result, .param = {}}
			);
			material_modes.emplace_back(
				model::Material::Mode{.alpha_mode = model::AlphaMode::Mask, .double_sided = true}
			);

			// Collect material textures
			for (const auto& [idx, material] : material_list.materials | std::views::enumerate)
			{
				const auto texture_index_result =
					texture_collector.add_material(context.device, texture_list, material.texture_set);
				if (!texture_index_result)
					return texture_index_result.error()
						.forward("Collect material texture failed", std::format("Material index: {}", idx));

				material_infos.emplace_back(
					MaterialInfo{.texture_index = *texture_index_result, .param = material.param}
				);
				material_modes.push_back(material.mode);
			}

			auto [textures, samplers, combined] = std::move(texture_collector).collect();

			if (combined.size() > MAX_TEXTURE_COUNT)
				return Error(
					"Too many textures",
					std::format("Texture count {} exceeds the maximum {}", combined.size(), MAX_TEXTURE_COUNT)
				);

			return std::make_tuple(
				std::move(textures),
				std::move(samplers),
				std::move(combined),
				std::move(material_infos),
				std::move(material_modes)
			);
		}

		std::expected<vk::raii::DescriptorPool, Error> create_descriptor_pool(
			const vk::raii::Device& device,
			size_t texture_count
		) noexcept
		{
			const auto pool_sizes = std::to_array<vk::DescriptorPoolSize>({
				{.type = vk::DescriptorType::eStorageBuffer,        .descriptorCount = 1},
				{.type = vk::DescriptorType::eCombinedImageSampler,
				 .descriptorCount = uint32_t(texture_count)                             },
			});
			const auto descriptor_pool_create_info =
				vk::DescriptorPoolCreateInfo()
					.setMaxSets(1)
					.setPoolSizes(pool_sizes)
					.setFlags(vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet);

			auto descriptor_pool_result =
				device.createDescriptorPool(descriptor_pool_create_info)
					.transform_error(Error::from<vk::Result>());
			if (!descriptor_pool_result)
				return descriptor_pool_result.error().forward("Create descriptor pool failed");

			return std::move(*descriptor_pool_result);
		}

		std::expected<vk::raii::DescriptorSet, Error> allocate_descriptor_set(
			const vk::raii::Device& device,
			vk::DescriptorPool pool,
			const MaterialLayout& layout,
			size_t texture_count
		) noexcept
		{
			const std::vector variable_descriptor_counts = {uint32_t(texture_count)};
			const auto variable_count_allocate_info =
				vk::DescriptorSetVariableDescriptorCountAllocateInfo()
					.setDescriptorCounts(variable_descriptor_counts);

			const auto descriptor_set_allocate_info =
				vk::DescriptorSetAllocateInfo{.descriptorPool = pool}
					.setPNext(&variable_count_allocate_info)
					.setSetLayouts(*layout.layout);

			auto descriptor_set_result =
				device.allocateDescriptorSets(descriptor_set_allocate_info)
					.transform_error(Error::from<vk::Result>());
			if (!descriptor_set_result)
				return descriptor_set_result.error().forward("Allocate descriptor set failed");

			return std::move(descriptor_set_result->front());
		}

		std::expected<void, Error> update_descriptor_set(
			const vk::raii::Device& device,
			vk::DescriptorSet descriptor_set,
			vulkan::ArrayBufferRef<MaterialInfo> info_buffer,
			const std::vector<std::pair<vk::ImageView, vk::Sampler>>& combined
		) noexcept
		{
			const auto binding0_buffer_info = vk::DescriptorBufferInfo{
				.buffer = info_buffer,
				.offset = 0,
				.range = info_buffer.size_vk(),
			};
			const auto binding0_write_info =
				vk::WriteDescriptorSet()
					.setDstSet(descriptor_set)
					.setDstBinding(0)
					.setDescriptorType(vk::DescriptorType::eStorageBuffer)
					.setBufferInfo(binding0_buffer_info);

			const auto binding1_image_infos =
				combined
				| std::views::transform([](const auto& pair) {
					  const auto& [texture, sampler] = pair;
					  return vk::DescriptorImageInfo{
						  .sampler = sampler,
						  .imageView = texture,
						  .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal,
					  };
				  })
				| std::ranges::to<std::vector>();
			const auto binding1_write_info =
				vk::WriteDescriptorSet()
					.setDstSet(descriptor_set)
					.setDstBinding(1)
					.setDescriptorType(vk::DescriptorType::eCombinedImageSampler)
					.setImageInfo(binding1_image_infos);

			device.updateDescriptorSets({binding0_write_info, binding1_write_info}, {});

			return {};
		}
	}

	std::pair<coro::task<std::expected<MaterialList, Error>>, std::shared_ptr<const MaterialList::Progress>>
	MaterialList::create(
		coro::thread_pool& thread_pool,
		const vulkan::DeviceContext& context,
		const MaterialLayout& layout,
		const model::MaterialList& material_list,
		TextureList::LoadOption texture_load_option
	) noexcept
	{
		auto progress = std::make_shared<Progress>(Progress::from<ProgressState::Preparing>());
		auto task = create_impl(thread_pool, progress, context, layout, material_list, texture_load_option);
		return {std::move(task), progress};
	}

	coro::task<std::expected<MaterialList, Error>> MaterialList::create_impl(
		coro::thread_pool& thread_pool,
		std::shared_ptr<Progress> progress,
		const vulkan::DeviceContext& context,
		const MaterialLayout& layout,
		const model::MaterialList& material_list,
		TextureList::LoadOption texture_load_option
	) noexcept
	{
		/* Step 1: Create texture list */
		util::Progress texture_list_progress;
		progress->set<ProgressState::TextureList>(texture_list_progress.get_ref());

		auto texture_list_result = co_await TextureList::create(
			thread_pool,
			std::move(texture_list_progress),
			context,
			material_list,
			texture_load_option
		);
		if (!texture_list_result) co_return texture_list_result.error().forward("Create texture list failed");
		auto texture_list = std::move(*texture_list_result);

		progress->set<ProgressState::Processing>(std::monostate());

		/* Step 2: Collect texture info */

		auto collect_result = collect_textures(context, texture_list, material_list);
		if (!collect_result) co_return collect_result.error().forward("Collect textures failed");
		auto [textures, samplers, combined, material_infos, material_modes] = std::move(*collect_result);

		/* Step 3: Create info buffer */

		auto info_buffer_result = create_info_buffer(context, material_infos);
		if (!info_buffer_result) co_return info_buffer_result.error().forward("Create info buffer failed");
		auto info_buffer = std::move(*info_buffer_result);

		/* Step 4: Create descriptor pool */

		auto descriptor_pool_result = create_descriptor_pool(context.device, combined.size());
		if (!descriptor_pool_result)
			co_return descriptor_pool_result.error().forward("Create descriptor pool failed");
		auto descriptor_pool = std::move(*descriptor_pool_result);

		/* Step 5: Allocate descriptor set */

		auto descriptor_set_result =
			allocate_descriptor_set(context.device, descriptor_pool, layout, combined.size());
		if (!descriptor_set_result)
			co_return descriptor_set_result.error().forward("Allocate descriptor set failed");
		auto descriptor_set = std::move(*descriptor_set_result);

		/* Step 6: Write descriptor set */

		if (const auto update_result =
				update_descriptor_set(context.device, descriptor_set, info_buffer, combined);
			!update_result)
			co_return update_result.error().forward("Update descriptor set failed");

		/* Done */

		co_return MaterialList(
			std::move(texture_list),
			std::move(textures),
			std::move(samplers),
			std::move(info_buffer),
			std::move(material_modes),
			std::move(descriptor_pool),
			std::move(descriptor_set)
		);
	}
}

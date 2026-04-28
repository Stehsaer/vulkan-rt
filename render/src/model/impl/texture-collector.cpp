#include "model/material-collector.hpp"
#include "model/vk-object.hpp"

namespace render::impl
{
	MaterialCollector::Result MaterialCollector::collect() && noexcept
	{
		return {
			.textures = std::move(textures),
			.samplers = std::move(samplers),
			.combined = std::move(combined)
		};
	}

	std::expected<TextureIndex, Error> MaterialCollector::add_material(
		const vk::raii::Device& device,
		const TextureList& texture_list,
		const model::TextureSet& texture_set
	) noexcept
	{
		const auto albedo_ref = texture_list.get_color_texture(texture_set.albedo);
		const auto emissive_ref = texture_list.get_color_texture(texture_set.emissive);
		const auto orm_ref = texture_list.get_color_texture(texture_set.roughness_metallic);
		const auto normal_ref = texture_list.get_normal_texture(texture_set.normal);

		const auto albedo_idx_result =
			add_texture(device, albedo_ref.texture, Texture::Usage::Color, albedo_ref.sample_mode);
		const auto emissive_idx_result =
			add_texture(device, emissive_ref.texture, Texture::Usage::Color, emissive_ref.sample_mode);
		const auto orm_idx_result =
			add_texture(device, orm_ref.texture, Texture::Usage::Linear, orm_ref.sample_mode);
		const auto normal_idx_result =
			add_texture(device, normal_ref.texture, Texture::Usage::Normal, normal_ref.sample_mode);

		if (!albedo_idx_result) return albedo_idx_result.error().forward("Collect albedo texture failed");
		if (!emissive_idx_result)
			return emissive_idx_result.error().forward("Collect emissive texture failed");
		if (!orm_idx_result) return orm_idx_result.error().forward("Collect ORM texture failed");
		if (!normal_idx_result) return normal_idx_result.error().forward("Collect normal texture failed");

		return TextureIndex{
			.albedo = *albedo_idx_result,
			.emissive = *emissive_idx_result,
			.orm = *orm_idx_result,
			.normal = *normal_idx_result
		};
	}

	std::expected<uint32_t, Error> MaterialCollector::add_texture(
		const vk::raii::Device& device,
		Texture::Ref texture_ref,
		Texture::Usage usage,
		model::SampleMode sample_mode
	) noexcept
	{
		uint32_t texture_index, sampler_index;

		// Get or create sampler
		if (!sampler_indices.contains(sample_mode))
		{
			auto sampler_result = impl::to_sampler(device, sample_mode);
			if (!sampler_result) return sampler_result.error().forward("Create sampler failed");
			auto sampler = std::move(*sampler_result);

			sampler_index = samplers.size();
			samplers.emplace_back(std::move(sampler));
			sampler_indices.emplace(sample_mode, sampler_index);
		}
		else
		{
			sampler_index = sampler_indices.at(sample_mode);
		}

		// Get or create texture
		if (!texture_indices.contains({texture_ref, usage}))
		{
			auto image_view_result = impl::to_image_view(device, texture_ref, usage);
			if (!image_view_result) return image_view_result.error().forward("Create image view failed");
			auto image_view = std::move(*image_view_result);

			texture_index = textures.size();
			textures.emplace_back(std::move(image_view));
			texture_indices.emplace(std::make_pair(texture_ref, usage), texture_index);
		}
		else
		{
			texture_index = texture_indices.at({texture_ref, usage});
		}

		// Get or create combined descriptor
		if (!combined_indices.contains({texture_index, sampler_index}))
		{
			const auto combined_index = combined.size();
			combined.emplace_back(textures[texture_index], samplers[sampler_index]);
			combined_indices.emplace(std::make_pair(texture_index, sampler_index), combined_index);
			return combined_index;
		}
		else
		{
			return combined_indices.at({texture_index, sampler_index});
		}
	}
}

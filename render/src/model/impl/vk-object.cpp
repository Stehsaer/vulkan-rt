#include "model/vk-object.hpp"
#include "model/enum-conv.hpp"

namespace render::impl
{
	std::expected<vk::raii::Sampler, Error> to_sampler(
		const vk::raii::Device& device,
		model::SampleMode sample_mode
	) noexcept
	{
		const auto mag_filter = as_filter(sample_mode.mag_filter);
		const auto min_filter = as_filter(sample_mode.min_filter);
		const auto mipmap_mode = as_mipmap_mode(sample_mode.mipmap_filter);
		const auto address_mode_u = as_address_mode(sample_mode.wrap_u);
		const auto address_mode_v = as_address_mode(sample_mode.wrap_v);

		const auto sampler_create_info = vk::SamplerCreateInfo{
			.magFilter = mag_filter,
			.minFilter = min_filter,
			.mipmapMode = mipmap_mode,
			.addressModeU = address_mode_u,
			.addressModeV = address_mode_v,
			.addressModeW = address_mode_v,  // Use V wrap mode for W as well
			.anisotropyEnable = vk::True,
			.maxAnisotropy = 4.0f,
			.maxLod = sample_mode.max_mipmap_level,
		};
		auto sampler_result =
			device.createSampler(sampler_create_info).transform_error(Error::from<vk::Result>());
		if (!sampler_result) return sampler_result.error().forward("Create sampler failed");
		return std::move(*sampler_result);
	}

	std::expected<vk::raii::ImageView, Error> to_image_view(
		const vk::raii::Device& device,
		Texture::Ref texture_ref,
		Texture::Usage usage
	) noexcept
	{
		const auto format = texture_ref.get_format(usage);
		if (format == vk::Format::eUndefined) return Error("Unsupported texture format and usage");

		const auto subresource_range = vk::ImageSubresourceRange{
			.aspectMask = vk::ImageAspectFlagBits::eColor,
			.baseMipLevel = 0,
			.levelCount = vk::RemainingMipLevels,
			.baseArrayLayer = 0,
			.layerCount = 1
		};

		const auto image_view_create_info = vk::ImageViewCreateInfo{
			.image = texture_ref.image,
			.viewType = vk::ImageViewType::e2D,
			.format = format,
			.subresourceRange = subresource_range
		};

		auto image_view_result =
			device.createImageView(image_view_create_info).transform_error(Error::from<vk::Result>());
		if (!image_view_result) return image_view_result.error().forward("Create image view failed");

		return std::move(*image_view_result);
	}
}

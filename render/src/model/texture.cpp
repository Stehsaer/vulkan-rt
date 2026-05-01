#include "render/model/texture.hpp"
#include "common/util/overload.hpp"
#include "image/bc-image.hpp"
#include "image/image.hpp"
#include "vulkan/util/static-resource-creator.hpp"

#include <map>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_structs.hpp>

namespace render
{
	std::expected<Texture, Error> Texture::load_rgba8_unorm(
		vulkan::StaticResourceCreator& resource_creator,
		const image::Image<image::Format::Unorm8, image::Layout::RGBA>& image,
		vk::ImageUsageFlags usage
	) noexcept
	{
		/* Generate mipmap chain */

		const auto mipmap_chain = image.resize_and_generate_mipmap(0);

		/* Create vulkan image */

		auto image_result = resource_creator.create_image_mipmap(
			mipmap_chain,
			vk::Format::eR8G8B8A8Unorm,
			usage,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::ImageCreateFlagBits::eMutableFormat
		);
		if (!image_result) return image_result.error().forward("Create image failed");
		auto vk_image = std::move(*image_result);

		/* Construct */

		return Texture{
			.image = std::move(vk_image),
			.format = Format::Rgba8Unorm,
			.mipmap_levels = uint32_t(mipmap_chain.size())
		};
	}

	std::expected<Texture, Error> Texture::load_bcn(
		vulkan::StaticResourceCreator& resource_creator,
		const image::Image<image::Format::Unorm8, image::Layout::RGBA>& image,
		image::BCnFormat format,
		vk::ImageUsageFlags usage
	) noexcept
	{
		/* Select Formats */

		const auto [texture_format, vk_format] = [format] -> std::pair<Format, vk::Format> {
			switch (format)
			{
			case image::BCnFormat::BC3:
				return {Format::BC3, vk::Format::eBc3UnormBlock};
			case image::BCnFormat::BC5:
				return {Format::BC5, vk::Format::eBc5SnormBlock};
			case image::BCnFormat::BC7:
				return {Format::BC7, vk::Format::eBc7UnormBlock};
			default:
				UNREACHABLE("Invalid format");
			}
		}();

		/* Generate mipmap */

		auto mipmap_chain_result =
			image.resize_and_generate_mipmap(2)
			| std::views::transform([format](const auto& image) {
				  return image::BCnImage::encode(image, format);
			  })
			| Error::collect();
		if (!mipmap_chain_result) return mipmap_chain_result.error().forward("Encode BCn image failed");
		const auto mipmap_chain = std::move(*mipmap_chain_result);

		/* Create vulkan image */

		auto image_result = resource_creator.create_image_mipmap_bcn(
			mipmap_chain,
			false,
			usage,
			vk::ImageLayout::eShaderReadOnlyOptimal,
			vk::ImageCreateFlagBits::eMutableFormat
		);
		if (!image_result) return image_result.error().forward("Create image failed");
		auto vk_image = std::move(*image_result);

		/* Construct */

		return Texture{
			.image = std::move(vk_image),
			.format = texture_format,
			.mipmap_levels = uint32_t(mipmap_chain.size())
		};
	}

	std::expected<Texture, Error> Texture::load_rg8_unorm(
		vulkan::StaticResourceCreator& resource_creator,
		const image::Image<image::Format::Unorm8, image::Layout::RGBA>& image,
		vk::ImageUsageFlags usage
	) noexcept
	{
		const auto mipmap_chain =
			image.map([](const glm::u8vec4& pixel) { return glm::u8vec2(pixel); })
				.resize_and_generate_mipmap(0);

		return resource_creator
			.create_image_mipmap(
				mipmap_chain,
				vk::Format::eR8G8Unorm,
				usage,
				vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::ImageCreateFlagBits::eMutableFormat
			)
			.transform_error(Error::forward_func("Create image failed"))
			.transform([&mipmap_chain](auto vk_image) {
				return Texture{
					.image = std::move(vk_image),
					.format = Format::Rg8Unorm,
					.mipmap_levels = uint32_t(mipmap_chain.size())
				};
			});
	}

	std::expected<Texture, Error> Texture::load_rg16_unorm(
		vulkan::StaticResourceCreator& resource_creator,
		const image::Image<image::Format::Unorm16, image::Layout::RGBA>& image,
		vk::ImageUsageFlags usage
	) noexcept
	{
		const auto mipmap_chain =
			image.map([](const glm::u16vec4& pixel) { return glm::u16vec2(pixel); })
				.resize_and_generate_mipmap(0);

		return resource_creator
			.create_image_mipmap(
				mipmap_chain,
				vk::Format::eR16G16Unorm,
				usage,
				vk::ImageLayout::eShaderReadOnlyOptimal,
				vk::ImageCreateFlagBits::eMutableFormat
			)
			.transform_error(Error::forward_func("Create image failed"))
			.transform([&mipmap_chain](auto vk_image) {
				return Texture{
					.image = std::move(vk_image),
					.format = Format::Rg16Unorm,
					.mipmap_levels = uint32_t(mipmap_chain.size())
				};
			});
	}

	std::expected<Texture, Error> Texture::load_color_texture(
		vulkan::StaticResourceCreator& resource_creator,
		const model::Texture& texture,
		ColorLoadStrategy load_strategy,
		vk::ImageUsageFlags usage
	) noexcept
	{
		auto image_result = texture.load_8bit();
		if (!image_result) return image_result.error().forward("Load texture failed");
		auto image = std::move(*image_result);

		switch (load_strategy)
		{
		case ColorLoadStrategy::Raw:
			return load_rgba8_unorm(resource_creator, image, usage);

		case ColorLoadStrategy::AllBC3:
			return load_bcn(resource_creator, image, image::BCnFormat::BC3, usage);

		case ColorLoadStrategy::AllBC7:
			return load_bcn(resource_creator, image, image::BCnFormat::BC7, usage);

		case ColorLoadStrategy::BalancedBC:
			return load_bcn(
				resource_creator,
				image,
				glm::max(image.size.x, image.size.y) <= BC7_THRESHOLD
					? image::BCnFormat::BC7
					: image::BCnFormat::BC3,
				usage
			);

		default:
			return Error(
				"Invalid load_strategy argument",
				std::format("Got load_strategy={}", std::to_underlying(load_strategy))
			);
		}
	}

	std::expected<Texture, Error> Texture::load_normal_texture(
		vulkan::StaticResourceCreator& resource_creator,
		const model::Texture& texture,
		NormalLoadStrategy load_strategy,
		vk::ImageUsageFlags usage
	) noexcept
	{
		using Unorm8Image = image::Image<image::Format::Unorm8, image::Layout::RGBA>;
		using Unorm16Image = image::Image<image::Format::Unorm16, image::Layout::RGBA>;

		auto image_result = texture.load();
		if (!image_result) return image_result.error().forward("Load texture failed");
		auto image = std::move(*image_result);

		const auto unorm16_to_unorm8 = [](const glm::u16vec4& pixel) {
			return glm::u8vec4(pixel >> uint16_t(8));
		};

		const auto convert_to_unorm8 = util::Overload(
			[](const Unorm8Image& image) { return image; },
			[&unorm16_to_unorm8](const Unorm16Image& image) { return image.map(unorm16_to_unorm8); }
		);

		switch (load_strategy)
		{
		case NormalLoadStrategy::AllUnorm8:
			return load_rg8_unorm(resource_creator, std::visit(convert_to_unorm8, image), usage);

		case NormalLoadStrategy::AdaptiveUnorm:
			return std::holds_alternative<Unorm16Image>(image)
				? load_rg16_unorm(resource_creator, std::get<Unorm16Image>(image), usage)
				: load_rg8_unorm(resource_creator, std::get<Unorm8Image>(image), usage);

		case NormalLoadStrategy::AdaptiveUnormBC5:
			return std::holds_alternative<Unorm16Image>(image)
				? load_rg16_unorm(resource_creator, std::get<Unorm16Image>(image), usage)
				: load_bcn(resource_creator, std::get<Unorm8Image>(image), image::BCnFormat::BC5, usage);

		case NormalLoadStrategy::AllBC5:
			return load_bcn(
				resource_creator,
				std::visit(convert_to_unorm8, image),
				image::BCnFormat::BC5,
				usage
			);

		default:
			UNREACHABLE("Invalid strategy", load_strategy);
		}
	}

	vk::Format Texture::Ref::get_format(Usage usage) noexcept
	{
		static const std::map<std::pair<Texture::Format, Usage>, vk::Format> format_map = {
			{{Texture::Format::Rgba8Unorm, Usage::Color},  vk::Format::eR8G8B8A8Srgb },
			{{Texture::Format::Rgba8Unorm, Usage::Linear}, vk::Format::eR8G8B8A8Unorm},
			{{Texture::Format::Rgba8Unorm, Usage::Normal}, vk::Format::eR8G8B8A8Unorm}, // error hint
			{{Texture::Format::BC3, Usage::Color},         vk::Format::eBc3SrgbBlock },
			{{Texture::Format::BC3, Usage::Linear},        vk::Format::eBc3UnormBlock},
			{{Texture::Format::BC7, Usage::Color},         vk::Format::eBc7SrgbBlock },
			{{Texture::Format::BC7, Usage::Linear},        vk::Format::eBc7UnormBlock},
			{{Texture::Format::Rg16Unorm, Usage::Normal},  vk::Format::eR16G16Unorm  },
			{{Texture::Format::Rg8Unorm, Usage::Normal},   vk::Format::eR8G8Unorm    },
			{{Texture::Format::BC5, Usage::Normal},        vk::Format::eBc5UnormBlock},
		};

		ASSERT(format_map.contains({format, usage}) && "Unsupported texture format and usage");

		return format_map.at({format, usage});
	}
}

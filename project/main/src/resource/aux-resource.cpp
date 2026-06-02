#include "resource/aux-resource.hpp"
#include "common/util/error.hpp"
#include "image/common.hpp"
#include "image/image.hpp"
#include "vulkan/interface/context.hpp"
#include "vulkan/numeric/base-level.hpp"
#include "vulkan/util/static-resource-creator.hpp"

#include <cstddef>
#include <expected>
#include <span>
#include <utility>
#include <vulkan/vulkan.hpp>

extern "C"
{
	extern const std::byte _binary_exposure_mask_png_start[];
	extern const std::byte _binary_exposure_mask_png_end[];

	static const std::span exposure_mask_data = {
		static_cast<const std::byte*>(_binary_exposure_mask_png_start),
		static_cast<const std::byte*>(_binary_exposure_mask_png_end)
	};
}

namespace resource
{
	std::expected<AuxResource, Error> AuxResource::create(const vulkan::Context& context) noexcept
	{
		auto exposure_mask_image_result =
			image::Image<image::Format::Unorm8, image::Layout::Grey>::decode(exposure_mask_data);
		if (!exposure_mask_image_result)
			return exposure_mask_image_result.error().forward("Decode exposure mask image failed");
		auto exposure_mask_image = std::move(*exposure_mask_image_result);

		auto resource_creator_result = vulkan::StaticResourceCreator::create(context);
		if (!resource_creator_result)
			return resource_creator_result.error().forward("Create resource creator failed");
		auto resource_creator = std::move(*resource_creator_result);

		auto exposure_mask_texture_result = resource_creator.create_image(
			context,
			exposure_mask_image,
			vk::Format::eR8Unorm,
			vk::ImageUsageFlagBits::eSampled
		);
		if (!exposure_mask_texture_result)
			return exposure_mask_texture_result.error().forward("Create exposure mask texture failed");
		auto exposure_mask_texture = std::move(*exposure_mask_texture_result);

		if (const auto result = resource_creator.execute_uploads(context); !result)
			return result.error().forward("Upload exposure mask texture failed");

		auto exposure_mask_view_result = context.device.createImageView(
			vk::ImageViewCreateInfo{
				.image = exposure_mask_texture,
				.viewType = vk::ImageViewType::e2D,
				.format = vk::Format::eR8Unorm,
				.subresourceRange = vulkan::base_level_image_range(vk::ImageAspectFlagBits::eColor)
			}
		);
		if (!exposure_mask_view_result) return Error::from(exposure_mask_view_result);
		auto exposure_mask_view = std::move(*exposure_mask_view_result);

		return AuxResource{
			.exposure_mask = std::move(exposure_mask_texture),
			.exposure_mask_view = std::move(exposure_mask_view),
		};
	}
}

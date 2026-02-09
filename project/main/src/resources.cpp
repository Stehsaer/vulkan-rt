#include "resources.hpp"
#include "image/raw-image.hpp"
#include "vertex.hpp"
#include "vulkan/util/uploader.hpp"

namespace
{
	constexpr auto vertex_buffer_data = std::to_array<Vertex>({
		Vertex{.position = {-0.5f, -0.5f}, .uv = {0.0f, 1.0f}},
		Vertex{.position = {0.5f, -0.5f},  .uv = {1.0f, 1.0f}},
		Vertex{.position = {0.5f, 0.5f},   .uv = {1.0f, 0.0f}},
		Vertex{.position = {-0.5f, -0.5f}, .uv = {0.0f, 1.0f}},
		Vertex{.position = {0.5f, 0.5f},   .uv = {1.0f, 0.0f}},
		Vertex{.position = {-0.5f, 0.5f},  .uv = {0.0f, 0.0f}}
	});

	constexpr auto image_format = vk::Format::eR8G8B8A8Srgb;
}

extern "C"
{
	extern const std::byte _binary_sample_jpg_start;
	extern const std::byte _binary_sample_jpg_end;
}

std::expected<Resources, Error> Resources::create(
	const vulkan::Context& context,
	vk::DescriptorSetLayout main_set_layout
) noexcept
{
	auto buffer_result = context.allocator.create_buffer(
		vk::BufferCreateInfo{
			.size = sizeof(Vertex) * vertex_buffer_data.size(),
			.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
			.sharingMode = vk::SharingMode::eExclusive
		},
		vulkan::alloc::MemoryUsage::GpuOnly
	);
	if (!buffer_result) return buffer_result.error().forward("Create vertex buffer failed");
	auto buffer = std::move(*buffer_result);

	auto image_data_result = (image::RawImage<image::Precision::Uint8, image::Layout::RGBA>::decode(
		std::span(&_binary_sample_jpg_start, &_binary_sample_jpg_end)
	));
	if (!image_data_result) return image_data_result.error().forward("Decode image failed");
	auto image_data = std::move(*image_data_result);

	const auto image_create_info = vk::ImageCreateInfo{
		.imageType = vk::ImageType::e2D,
		.format = image_format,
		.extent = vk::Extent3D{.width = image_data.size.x, .height = image_data.size.y, .depth = 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.samples = vk::SampleCountFlagBits::e1,
		.tiling = vk::ImageTiling::eOptimal,
		.usage = vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		.sharingMode = vk::SharingMode::eExclusive,
		.initialLayout = vk::ImageLayout::eUndefined
	};
	auto image_result =
		context.allocator.create_image(image_create_info, vulkan::alloc::MemoryUsage::GpuOnly);
	if (!image_result) return image_result.error().forward("Create image failed");
	auto image = std::move(*image_result);

	const auto image_view_create_info = vk::ImageViewCreateInfo{
		.image = image,
		.viewType = vk::ImageViewType::e2D,
		.format = image_format,
		.components = {},
		.subresourceRange = {
					   .aspectMask = vk::ImageAspectFlagBits::eColor,
					   .baseMipLevel = 0,
					   .levelCount = 1,
					   .baseArrayLayer = 0,
					   .layerCount = 1
		}
	};
	auto image_view_result =
		context.device.createImageView(image_view_create_info).transform_error(Error::from<vk::Result>());
	if (!image_view_result) return image_view_result.error().forward("Create image view failed");
	auto image_view = std::move(*image_view_result);

	const auto sampler_create_info = vk::SamplerCreateInfo{
		.magFilter = vk::Filter::eLinear,
		.minFilter = vk::Filter::eLinear,
		.mipmapMode = vk::SamplerMipmapMode::eLinear,
		.addressModeU = vk::SamplerAddressMode::eRepeat,
		.addressModeV = vk::SamplerAddressMode::eRepeat,
		.addressModeW = vk::SamplerAddressMode::eRepeat,
		.mipLodBias = 0.0f,
		.anisotropyEnable = vk::False,
		.maxAnisotropy = 1.0f,
		.compareEnable = vk::False,
		.compareOp = vk::CompareOp::eAlways,
		.minLod = 0.0f,
		.maxLod = VK_LOD_CLAMP_NONE,
		.borderColor = vk::BorderColor::eIntOpaqueBlack,
		.unnormalizedCoordinates = vk::False,
	};
	auto sampler_result =
		context.device.createSampler(sampler_create_info).transform_error(Error::from<vk::Result>());
	if (!sampler_result) return sampler_result.error().forward("Create sampler failed");
	auto sampler = std::move(*sampler_result);

	const auto pool_sizes = std::to_array<vk::DescriptorPoolSize>({
		{.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 10}
	});

	const auto descriptor_pool_create_info =
		vk::DescriptorPoolCreateInfo{
			.flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
			.maxSets = 10,
		}
			.setPoolSizes(pool_sizes);

	auto descriptor_pool_result =
		context.device.createDescriptorPool(descriptor_pool_create_info)
			.transform_error(Error::from<vk::Result>());
	if (!descriptor_pool_result)
		return descriptor_pool_result.error().forward("Create descriptor pool failed");
	auto descriptor_pool = std::move(*descriptor_pool_result);

	const auto descriptor_set_layouts = std::to_array<vk::DescriptorSetLayout>({main_set_layout});
	auto descriptor_set_vec_result =
		context.device
			.allocateDescriptorSets(
				vk::DescriptorSetAllocateInfo{.descriptorPool = *descriptor_pool, .descriptorSetCount = 1}
					.setSetLayouts(descriptor_set_layouts)
			)
			.transform_error(Error::from<vk::Result>());
	if (!descriptor_set_vec_result)
		return descriptor_set_vec_result.error().forward("Allocate descriptor sets failed");
	auto descriptor_set_vec = std::move(*descriptor_set_vec_result);

	const auto descriptor_image_info = vk::DescriptorImageInfo{
		.sampler = *sampler,
		.imageView = *image_view,
		.imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal
	};

	context.device.updateDescriptorSets(
		vk::WriteDescriptorSet{
			.dstSet = descriptor_set_vec[0],
			.dstBinding = 0,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType = vk::DescriptorType::eCombinedImageSampler
		}
			.setImageInfo(descriptor_image_info),
		{}
	);

	vulkan::util::Uploader
		uploader(context.device, *context.queues.graphics, context.queues.graphics_index, context.allocator);

	const auto image_upload_task = vulkan::util::Uploader::ImageUploadParam{
		.dst_image = image,
		.data = util::as_bytes(image_data.data),
		.buffer_row_length = image_data.size.x,
		.buffer_image_height = 0,
		.subresource_layers =
			vk::ImageSubresourceLayers{
									   .aspectMask = vk::ImageAspectFlagBits::eColor,
									   .mipLevel = 0,
									   .baseArrayLayer = 0,
									   .layerCount = 1
			},
		.image_extent = vk::Extent3D{.width = image_data.size.x, .height = image_data.size.y, .depth = 1},
		.dst_layout = vk::ImageLayout::eShaderReadOnlyOptimal
	};

	const auto buffer_upload_task = vulkan::util::Uploader::BufferUploadParam{
		.dst_buffer = buffer,
		.data = util::as_bytes(vertex_buffer_data)
	};

	if (const auto result = uploader.upload_image(image_upload_task); !result)
		return result.error().forward("Add image upload task failed");

	if (const auto upload_task_result = uploader.upload_buffer(buffer_upload_task); !upload_task_result)
		return upload_task_result.error().forward("Add vertex buffer upload task failed");

	if (const auto upload_result = uploader.execute(); !upload_result)
		return upload_result.error().forward("Upload image data failed");

	return Resources{
		.vertex_buffer = std::move(buffer),
		.texture = std::move(image),
		.texture_view = std::move(image_view),
		.texture_sampler = std::move(sampler),
		.main_descriptor_pool = std::move(descriptor_pool),
		.main_descriptor_set = std::move(descriptor_set_vec[0]),
	};
}
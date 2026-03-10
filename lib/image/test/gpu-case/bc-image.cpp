#include "common/file.hpp"
#include <filesystem>
#define DOCTEST_CONFIG_IMPLEMENT

#include "common/test-macro.hpp"
#include "common/util/span.hpp"
#include "image/bc-image.hpp"
#include "image/image.hpp"
#include "test-asset.hpp"
#include "vulkan/alloc.hpp"
#include "vulkan/context/device.hpp"
#include "vulkan/context/instance.hpp"

#include <doctest.h>
#include <glm/fwd.hpp>
#include <print>
#include <vulkan/vulkan_enums.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_structs.hpp>

struct Context
{
	vulkan::HeadlessInstanceContext instance_context;
	vulkan::HeadlessDeviceContext device_context;

	static std::expected<Context, Error> create() noexcept
	{
		const auto instance_config = vulkan::InstanceConfig{.validation = true};
		const auto device_config = vulkan::DeviceConfig();

		auto instance_context_result = vulkan::HeadlessInstanceContext::create(instance_config);
		if (!instance_context_result)
			return instance_context_result.error().forward("Create instance context failed");
		auto instance_context = std::move(*instance_context_result);

		auto device_context_result = vulkan::HeadlessDeviceContext::create(instance_context, device_config);
		if (!device_context_result)
			return device_context_result.error().forward("Create device context failed");
		auto device_context = std::move(*device_context_result);

		return Context{
			.instance_context = std::move(instance_context),
			.device_context = std::move(device_context)
		};
	}
};

static const Context* context_ptr;
static std::filesystem::path temp_directory;

int main(int argc, const char* const* argv)
{
	auto context_result = Context::create();
	if (!context_result)
	{
		std::println("Initialize test failed");
		std::println("{}", context_result.error().chain());
		return EXIT_FAILURE;
	}
	const auto context = std::make_unique<Context>(std::move(*context_result));
	context_ptr = context.get();

	temp_directory = std::filesystem::path(std::getenv("TEMP_DIR"));
	if (!std::filesystem::exists(temp_directory) || !std::filesystem::is_directory(temp_directory))
	{
		std::println("Temporary path {} doesn't exist or is not a directory", temp_directory.string());
		return EXIT_FAILURE;
	}
	std::println("Images will be saved to {} for inspection", temp_directory.string());

	const auto result = doctest::Context(argc, argv).run();

	return result;
}

using ImageType = image::Image<image::Format::Unorm8, image::Layout::RGBA>;

static void test_common(
	glm::u32vec2 size,
	std::span<const std::byte> upload_data,
	std::span<std::byte> readback_data,
	vk::Format bc_format,
	vk::Format raw_format
)
{
	const auto& device = context_ptr->device_context->device;
	const auto& allocator = context_ptr->device_context->allocator;

	// Src buffer

	const auto src_buffer_create_info = vk::BufferCreateInfo{
		.size = upload_data.size_bytes(),
		.usage = vk::BufferUsageFlagBits::eTransferSrc
	};
	auto src_buffer_result =
		allocator.create_buffer(src_buffer_create_info, vulkan::alloc::MemoryUsage::CpuToGpu);
	EXPECT_SUCCESS(src_buffer_result);
	auto src_buffer = std::move(*src_buffer_result);

	const auto upload_result = src_buffer.upload(upload_data);
	EXPECT_SUCCESS(upload_result);

	// Dst buffer

	const auto dst_buffer_create_info = vk::BufferCreateInfo{
		.size = readback_data.size_bytes(),
		.usage = vk::BufferUsageFlagBits::eTransferDst
	};
	auto dst_buffer_result =
		allocator.create_buffer(dst_buffer_create_info, vulkan::alloc::MemoryUsage::GpuToCpu);
	EXPECT_SUCCESS(dst_buffer_result);
	auto dst_buffer = std::move(*dst_buffer_result);

	// Src image

	const auto src_image_create_info = vk::ImageCreateInfo{
		.imageType = vk::ImageType::e2D,
		.format = bc_format,
		.extent = {.width = size.x, .height = size.y, .depth = 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
	};
	auto src_image_result =
		allocator.create_image(src_image_create_info, vulkan::alloc::MemoryUsage::GpuOnly);
	EXPECT_SUCCESS(src_image_result);
	auto src_image = std::move(*src_image_result);

	// Dst image

	const auto dst_image_create_info = vk::ImageCreateInfo{
		.imageType = vk::ImageType::e2D,
		.format = raw_format,
		.extent = {.width = size.x, .height = size.y, .depth = 1},
		.mipLevels = 1,
		.arrayLayers = 1,
		.usage = vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst
	};
	auto dst_image_result =
		allocator.create_image(dst_image_create_info, vulkan::alloc::MemoryUsage::GpuOnly);
	EXPECT_SUCCESS(dst_image_result);
	auto dst_image = std::move(*dst_image_result);

	// Command buffer

	auto command_pool_result =
		device
			.createCommandPool(
				{.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
				 .queueFamilyIndex = context_ptr->device_context->render_queue.family_index}
			)
			.transform_error(Error::from<vk::Result>());
	EXPECT_SUCCESS(command_pool_result);
	auto command_pool = std::move(*command_pool_result);

	auto command_buffer_result =
		device
			.allocateCommandBuffers(
				vk::CommandBufferAllocateInfo{
					.commandPool = command_pool,
					.level = vk::CommandBufferLevel::ePrimary,
					.commandBufferCount = 1,
				}
			)
			.transform_error(Error::from<vk::Result>())
			.transform([](std::vector<vk::raii::CommandBuffer> list) { return std::move(list[0]); });
	EXPECT_SUCCESS(command_buffer_result);
	auto command_buffer = std::move(*command_buffer_result);

	// Fence

	auto fence_result = device.createFence(vk::FenceCreateInfo()).transform_error(Error::from<vk::Result>());
	EXPECT_SUCCESS(fence_result);
	auto fence = std::move(*fence_result);

	command_buffer.begin(vk::CommandBufferBeginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit});
	{
		// Copy from src_buffer to src_image

		const auto src_image_barrier_pre = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
			.srcAccessMask = {},
			.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eTransferDstOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = src_image,
			.subresourceRange = {
							  .aspectMask = vk::ImageAspectFlagBits::eColor,
							  .baseMipLevel = 0,
							  .levelCount = 1,
							  .baseArrayLayer = 0,
							  .layerCount = 1,
							  },
		};

		const auto src_copy_region = vk::BufferImageCopy{
			.bufferOffset = 0,
			.bufferRowLength = 0,
			.bufferImageHeight = 0,
			.imageSubresource =
				{.aspectMask = vk::ImageAspectFlagBits::eColor,
								   .mipLevel = 0,
								   .baseArrayLayer = 0,
								   .layerCount = 1},
			.imageOffset = {.x = 0, .y = 0, .z = 0},
			.imageExtent = {.width = size.x, .height = size.y, .depth = 1}
		};

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(src_image_barrier_pre));
		command_buffer.copyBufferToImage(
			src_buffer,
			src_image,
			vk::ImageLayout::eTransferDstOptimal,
			{src_copy_region}
		);

		// Blit

		const auto src_image_barrier_post = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = vk::ImageLayout::eTransferSrcOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = src_image,
			.subresourceRange = {
								 .aspectMask = vk::ImageAspectFlagBits::eColor,
								 .baseMipLevel = 0,
								 .levelCount = 1,
								 .baseArrayLayer = 0,
								 .layerCount = 1,
								 },
		};

		const auto dst_image_barrier_pre = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eTopOfPipe,
			.srcAccessMask = {},
			.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.dstAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.oldLayout = vk::ImageLayout::eUndefined,
			.newLayout = vk::ImageLayout::eTransferDstOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = dst_image,
			.subresourceRange = {
							  .aspectMask = vk::ImageAspectFlagBits::eColor,
							  .baseMipLevel = 0,
							  .levelCount = 1,
							  .baseArrayLayer = 0,
							  .layerCount = 1,
							  },
		};

		const auto blit_barriers = std::to_array({src_image_barrier_post, dst_image_barrier_pre});

		const auto blit_offset = std::to_array({
			vk::Offset3D{.x = 0,               .y = 0,               .z = 0},
			vk::Offset3D{.x = int32_t(size.x), .y = int32_t(size.y), .z = 1}
		});

		const auto blit_region = vk::ImageBlit{
			.srcSubresource =
				{.aspectMask = vk::ImageAspectFlagBits::eColor,
								 .mipLevel = 0,
								 .baseArrayLayer = 0,
								 .layerCount = 1},
			.srcOffsets = blit_offset,
			.dstSubresource =
				{.aspectMask = vk::ImageAspectFlagBits::eColor,
								 .mipLevel = 0,
								 .baseArrayLayer = 0,
								 .layerCount = 1},
			.dstOffsets = blit_offset
		};

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(blit_barriers));
		command_buffer.blitImage(
			src_image,
			vk::ImageLayout::eTransferSrcOptimal,
			dst_image,
			vk::ImageLayout::eTransferDstOptimal,
			{blit_region},
			vk::Filter::eNearest
		);

		// Copy back

		const auto dst_image_barrier_post = vk::ImageMemoryBarrier2{
			.srcStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.srcAccessMask = vk::AccessFlagBits2::eTransferWrite,
			.dstStageMask = vk::PipelineStageFlagBits2::eTransfer,
			.dstAccessMask = vk::AccessFlagBits2::eTransferRead,
			.oldLayout = vk::ImageLayout::eTransferDstOptimal,
			.newLayout = vk::ImageLayout::eTransferSrcOptimal,
			.srcQueueFamilyIndex = vk::QueueFamilyIgnored,
			.dstQueueFamilyIndex = vk::QueueFamilyIgnored,
			.image = dst_image,
			.subresourceRange = {
								 .aspectMask = vk::ImageAspectFlagBits::eColor,
								 .baseMipLevel = 0,
								 .levelCount = 1,
								 .baseArrayLayer = 0,
								 .layerCount = 1,
								 },
		};

		const auto dst_copy_region = vk::BufferImageCopy{
			.imageSubresource =
				{.aspectMask = vk::ImageAspectFlagBits::eColor,
								   .mipLevel = 0,
								   .baseArrayLayer = 0,
								   .layerCount = 1},
			.imageOffset = {.x = 0, .y = 0, .z = 0},
			.imageExtent = {.width = size.x, .height = size.y, .depth = 1}
		};

		command_buffer.pipelineBarrier2(vk::DependencyInfo().setImageMemoryBarriers(dst_image_barrier_post));
		command_buffer.copyImageToBuffer(
			dst_image,
			vk::ImageLayout::eTransferSrcOptimal,
			dst_buffer,
			{dst_copy_region}
		);
	}
	command_buffer.end();

	const auto command_buffers = std::to_array<vk::CommandBuffer>({command_buffer});
	context_ptr->device_context->render_queue.queue
		->submit(vk::SubmitInfo().setCommandBuffers(command_buffers), fence);

	const auto wait_result = device.waitForFences({fence}, vk::True, 1'000'000'000);
	if (wait_result != vk::Result::eSuccess)
	{
		if (wait_result == vk::Result::eTimeout) FAIL("Fence wait timeout");
		FAIL("Wait for fence failed");
	}

	const auto readback_result = dst_buffer.download(readback_data);
	EXPECT_SUCCESS(readback_result);

	device.waitIdle();
}

static float rgba_max_error(const glm::u8vec4& pix1, const glm::u8vec4& pix2) noexcept
{
	const glm::f32vec4 diff = glm::abs(glm::f32vec4(pix1) - glm::f32vec4(pix2));
	return std::max({diff.r, diff.g, diff.b, diff.a});
}

static float rgba_mse(const glm::u8vec4& pix1, const glm::u8vec4& pix2) noexcept
{
	const glm::f32vec4 diff = glm::f32vec4(pix1) - glm::f32vec4(pix2);
	const glm::f32vec4 diff2 = diff * diff;
	return (diff2.r + diff2.g + diff2.b + diff2.a) / 4.0f;
}

static float rg_max_error(const glm::u8vec4& pix1, const glm::u8vec2& pix2) noexcept
{
	const glm::f32vec2 diff = glm::abs(glm::f32vec2(pix1) - glm::f32vec2(pix2));
	return std::max({diff.r, diff.g});
}

static float rg_mse(const glm::u8vec4& pix1, const glm::u8vec2& pix2) noexcept
{
	const glm::f32vec2 diff = glm::f32vec2(pix1) - glm::f32vec2(pix2);
	const glm::f32vec2 diff2 = diff * diff;
	return (diff2.r + diff2.g) / 2.0f;
}

static void check_bc3(const std::string_view& name, std::span<const std::byte> image)
{
	auto decoded_image_result = ImageType::decode(image);
	EXPECT_SUCCESS(decoded_image_result);
	const auto decoded_image = std::move(decoded_image_result.value());

	ImageType readback_image(decoded_image.size);

	auto bc3_image_result = image::BCnImage::encode(decoded_image, image::BCnFormat::BC3);
	EXPECT_SUCCESS(bc3_image_result);
	auto bc3_image = std::move(*bc3_image_result);

	test_common(
		decoded_image.size,
		util::as_bytes(bc3_image.data),
		util::as_writable_bytes(readback_image.data),
		vk::Format::eBc3UnormBlock,
		vk::Format::eR8G8B8A8Unorm
	);

	const auto squared_error = std::views::zip_transform(rgba_mse, decoded_image.data, readback_image.data);
	const auto max_error = std::views::zip_transform(rgba_max_error, decoded_image.data, readback_image.data);

	const auto mse = std::ranges::fold_left_first(squared_error, std::plus()).value()
		/ (decoded_image.size.x * decoded_image.size.y);
	const auto max_err = std::ranges::max(max_error);

	std::println("(BC3, {}) Measured MSE: {:.2f}, Max error: {}", name, mse, max_err);

	const auto write_result =
		readback_image.encode(image::encode_format::Png())
			.and_then([&name](const std::span<const std::byte>& data) {
				return file::write(temp_directory / std::format("BC3-{}.png", name), data);
			});
	EXPECT_SUCCESS(write_result);

	if (mse > 40.0f) FAIL("MSE too large");
	if (max_err > 100.0f) FAIL("Max error too large");
}

static void check_bc5(const std::string_view& name, std::span<const std::byte> image)
{
	auto decoded_image_result = ImageType::decode(image);
	EXPECT_SUCCESS(decoded_image_result);
	const auto decoded_image = std::move(decoded_image_result.value());

	image::Image<image::Format::Unorm8, image::Layout::RG> readback_image(decoded_image.size);

	auto bc5_image_result = image::BCnImage::encode(decoded_image, image::BCnFormat::BC5);
	EXPECT_SUCCESS(bc5_image_result);
	auto bc5_image = std::move(*bc5_image_result);

	test_common(
		decoded_image.size,
		util::as_bytes(bc5_image.data),
		util::as_writable_bytes(readback_image.data),
		vk::Format::eBc5UnormBlock,
		vk::Format::eR8G8Unorm
	);

	const auto squared_error = std::views::zip_transform(rg_mse, decoded_image.data, readback_image.data);
	const auto max_error = std::views::zip_transform(rg_max_error, decoded_image.data, readback_image.data);

	const auto mse = std::ranges::fold_left_first(squared_error, std::plus()).value()
		/ (decoded_image.size.x * decoded_image.size.y);
	const auto max_err = std::ranges::max(max_error);

	std::println("(BC5, {}) Measured MSE: {:.2f}, Max error: {}", name, mse, max_err);

	const auto write_result =
		readback_image.map([](const glm::u8vec2& pixel) { return glm::u8vec4(pixel.r, pixel.g, 0, 255); })
			.encode(image::encode_format::Png())
			.and_then([&name](const std::span<const std::byte>& data) {
				return file::write(temp_directory / std::format("BC5-{}.png", name), data);
			});
	EXPECT_SUCCESS(write_result);

	if (mse > 30.0f) FAIL("MSE too large");
	if (max_err > 50.0f) FAIL("Max error too large");
}

static void check_bc7(const std::string_view& name, std::span<const std::byte> image)
{
	auto decoded_image_result = ImageType::decode(image);
	EXPECT_SUCCESS(decoded_image_result);
	const auto decoded_image = std::move(decoded_image_result.value());

	ImageType readback_image(decoded_image.size);

	auto bc7_image_result = image::BCnImage::encode(decoded_image, image::BCnFormat::BC7);
	EXPECT_SUCCESS(bc7_image_result);
	auto bc7_image = std::move(*bc7_image_result);

	test_common(
		decoded_image.size,
		util::as_bytes(bc7_image.data),
		util::as_writable_bytes(readback_image.data),
		vk::Format::eBc7UnormBlock,
		vk::Format::eR8G8B8A8Unorm
	);

	const auto squared_error = std::views::zip_transform(rgba_mse, decoded_image.data, readback_image.data);
	const auto max_error = std::views::zip_transform(rgba_max_error, decoded_image.data, readback_image.data);

	const auto mse = std::ranges::fold_left_first(squared_error, std::plus()).value()
		/ (decoded_image.size.x * decoded_image.size.y);
	const auto max_err = std::ranges::max(max_error);

	std::println("(BC7, {}) Measured MSE: {:.2f}, Max error: {}", name, mse, max_err);

	const auto write_result =
		readback_image.encode(image::encode_format::Png())
			.and_then([&name](const std::span<const std::byte>& data) {
				return file::write(temp_directory / std::format("BC7-{}.png", name), data);
			});
	EXPECT_SUCCESS(write_result);

	if (mse > 20.0f) FAIL("MSE too large");
	if (max_err > 30.0f) FAIL("Max error too large");
}

TEST_CASE("BC3")
{
	check_bc3("Checker", checker_image_data);
	check_bc3("Complex", complex_image_data);
}

TEST_CASE("BC5")
{
	check_bc5("Checker", checker_image_data);
	check_bc5("Complex", complex_image_data);
}

TEST_CASE("BC7")
{
	check_bc7("Checker", checker_image_data);
	check_bc7("Complex", complex_image_data);
}

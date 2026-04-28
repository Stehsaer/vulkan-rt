#include "common/file.hpp"
#include "vulkan/util/static-resource-creator.hpp"
#include <filesystem>
#define DOCTEST_CONFIG_IMPLEMENT

#include "common/test-macro.hpp"
#include "image/bc-image.hpp"
#include "image/image.hpp"
#include "test-asset.hpp"
#include "vulkan/context/device.hpp"
#include "vulkan/context/instance.hpp"
#include "vulkan/util/resource-readback.hpp"

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
		const auto device_config = vulkan::DeviceOption();

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

	auto bc3_image_result = image::BCnImage::encode(decoded_image, image::BCnFormat::BC3);
	EXPECT_SUCCESS(bc3_image_result);
	auto bc3_image = std::move(*bc3_image_result);

	vulkan::StaticResourceCreator resource_creator(context_ptr->device_context.get());

	auto gpu_image_result = resource_creator.create_image_bcn(
		bc3_image,
		false,
		vk::ImageUsageFlagBits::eTransferSrc,
		vk::ImageLayout::eTransferSrcOptimal
	);
	EXPECT_SUCCESS(gpu_image_result);
	auto gpu_image = std::move(*gpu_image_result);

	const auto upload_result = resource_creator.execute_uploads();
	EXPECT_SUCCESS(upload_result);

	auto readback_image_result = vulkan::readback_image<image::Format::Unorm8, image::Layout::RGBA>(
		context_ptr->device_context.get(),
		gpu_image,
		vk::ImageLayout::eTransferSrcOptimal,
		vk::Format::eR8G8B8A8Unorm,
		decoded_image.size
	);
	EXPECT_SUCCESS(readback_image_result);
	const auto readback_image = std::move(*readback_image_result);

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

	auto bc5_image_result = image::BCnImage::encode(decoded_image, image::BCnFormat::BC5);
	EXPECT_SUCCESS(bc5_image_result);
	auto bc5_image = std::move(*bc5_image_result);

	vulkan::StaticResourceCreator resource_creator(context_ptr->device_context.get());

	auto gpu_image_result = resource_creator.create_image_bcn(
		bc5_image,
		false,
		vk::ImageUsageFlagBits::eTransferSrc,
		vk::ImageLayout::eTransferSrcOptimal
	);
	EXPECT_SUCCESS(gpu_image_result);
	auto gpu_image = std::move(*gpu_image_result);

	const auto upload_result = resource_creator.execute_uploads();
	EXPECT_SUCCESS(upload_result);

	auto readback_image_result = vulkan::readback_image<image::Format::Unorm8, image::Layout::RG>(
		context_ptr->device_context.get(),
		gpu_image,
		vk::ImageLayout::eTransferSrcOptimal,
		vk::Format::eR8G8Unorm,
		decoded_image.size
	);
	EXPECT_SUCCESS(readback_image_result);
	const auto readback_image = std::move(*readback_image_result);

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

	auto bc7_image_result = image::BCnImage::encode(decoded_image, image::BCnFormat::BC7);
	EXPECT_SUCCESS(bc7_image_result);
	auto bc7_image = std::move(*bc7_image_result);

	vulkan::StaticResourceCreator resource_creator(context_ptr->device_context.get());

	auto gpu_image_result = resource_creator.create_image_bcn(
		bc7_image,
		false,
		vk::ImageUsageFlagBits::eTransferSrc,
		vk::ImageLayout::eTransferSrcOptimal
	);
	EXPECT_SUCCESS(gpu_image_result);
	auto gpu_image = std::move(*gpu_image_result);

	const auto upload_result = resource_creator.execute_uploads();
	EXPECT_SUCCESS(upload_result);

	auto readback_image_result = vulkan::readback_image<image::Format::Unorm8, image::Layout::RGBA>(
		context_ptr->device_context.get(),
		gpu_image,
		vk::ImageLayout::eTransferSrcOptimal,
		vk::Format::eR8G8B8A8Unorm,
		decoded_image.size
	);
	EXPECT_SUCCESS(readback_image_result);
	const auto readback_image = std::move(*readback_image_result);

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

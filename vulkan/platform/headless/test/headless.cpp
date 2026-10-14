#include "common/test-macro.hpp"
#include "vulkan/common/context.hpp"
#include "vulkan/platform/headless/device.hpp"
#include "vulkan/platform/headless/instance.hpp"

#include <doctest.h>
#include <utility>

static void test_create(
	const vulkan::InstanceFeature& instance_feature,
	const vulkan::DeviceFeature& device_config
) noexcept
{
	auto instance_result = vulkan::HeadlessInstance::create(vulkan::InstanceProperty{}, instance_feature);
	EXPECT_SUCCESS(instance_result);
	auto instance = std::move(*instance_result);

	auto device_result = vulkan::HeadlessDevice::create(instance, device_config);
	EXPECT_SUCCESS(device_result);
	auto device = std::move(*device_result);
}

TEST_CASE("No validation layer")
{
	const auto instance_feature = vulkan::InstanceFeature{.validation = false};
	const auto device_config = vulkan::DeviceFeature();

	test_create(instance_feature, device_config);
}

TEST_CASE("Validation layer")
{
	const auto instance_feature = vulkan::InstanceFeature{.validation = true};
	const auto device_config = vulkan::DeviceFeature();

	test_create(instance_feature, device_config);
}

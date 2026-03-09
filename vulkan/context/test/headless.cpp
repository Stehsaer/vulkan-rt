#include "common/test-macro.hpp"
#include "vulkan/context/device.hpp"
#include "vulkan/context/instance.hpp"

#include <doctest.h>

static void test_create(
	const vulkan::InstanceConfig& instance_config,
	const vulkan::DeviceConfig& device_config
) noexcept
{
	auto instance_context_result = vulkan::HeadlessInstanceContext::create(instance_config);
	EXPECT_SUCCESS(instance_context_result);
	auto instance_context = std::move(*instance_context_result);

	auto device_context_result = vulkan::HeadlessDeviceContext::create(instance_context, device_config);
	EXPECT_SUCCESS(device_context_result);
	auto device_context = std::move(*device_context_result);
}

TEST_CASE("No validation layer")
{
	const auto instance_config = vulkan::InstanceConfig();
	const auto device_config = vulkan::DeviceConfig();

	test_create(instance_config, device_config);
}

TEST_CASE("Validation layer")
{
	const auto instance_config = vulkan::InstanceConfig{.validation = true};
	const auto device_config = vulkan::DeviceConfig();

	test_create(instance_config, device_config);
}

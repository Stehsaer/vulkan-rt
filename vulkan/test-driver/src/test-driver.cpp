#include "vulkan/test-driver.hpp"
#include "vulkan/common/context.hpp"
#include "vulkan/platform/headless/device.hpp"
#include "vulkan/platform/headless/instance.hpp"

#include <cstdlib>
#include <libassert/assert.hpp>
#include <utility>

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <print>

namespace vulkan
{
	static const vulkan::HeadlessDevice* test_context = nullptr;

	const vulkan::HeadlessDevice& get_test_context() noexcept
	{
		ASSERT(test_context != nullptr);
		return *test_context;
	}
}

int main(int argc, const char** argv) noexcept
{
	const vulkan::InstanceFeature instance_feature = {.validation = true};
	const vulkan::DeviceFeature device_option = {};

	auto instance_result = vulkan::HeadlessInstance::create(vulkan::InstanceProperty{}, instance_feature);
	if (!instance_result)
	{
		std::println("Initialize vulkan instance failed: {}", instance_result.error().root());
		return EXIT_FAILURE;
	}
	auto instance = std::move(*instance_result);

	auto device_result = vulkan::HeadlessDevice::create(instance, device_option);
	if (!device_result)
	{
		std::println("Initialize vulkan device failed: {}", device_result.error().root());
		return EXIT_FAILURE;
	}
	auto device = std::move(*device_result);

	vulkan::test_context = &device;

	return doctest::Context(argc, argv).run();
}

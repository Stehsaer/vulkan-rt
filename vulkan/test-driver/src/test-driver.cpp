#include "vulkan/test-driver.hpp"
#include "vulkan/context/instance.hpp"

#define DOCTEST_CONFIG_IMPLEMENT
#include <doctest.h>
#include <print>

namespace vulkan
{
	static const vulkan::HeadlessDeviceContext* test_context = nullptr;

	const vulkan::HeadlessDeviceContext& get_test_context() noexcept
	{
		assert(test_context != nullptr);
		return *test_context;
	}
}

int main(int argc, const char** argv) noexcept
{
	const vulkan::InstanceConfig instance_config = {.validation = true};
	const vulkan::DeviceOption device_option = {};

	auto instance_result = vulkan::HeadlessInstanceContext::create(instance_config);
	if (!instance_result)
	{
		std::println("Initialize vulkan instance failed: {}", instance_result.error().root());
		return EXIT_FAILURE;
	}
	auto instance = std::move(*instance_result);

	auto device_result = vulkan::HeadlessDeviceContext::create(instance, device_option);
	if (!device_result)
	{
		std::println("Initialize vulkan device failed: {}", device_result.error().root());
		return EXIT_FAILURE;
	}
	auto device = std::move(*device_result);

	vulkan::test_context = &device;

	return doctest::Context(argc, argv).run();
}

#include "vulkan/util/linked-struct.hpp"

#include <doctest.h>
#include <vulkan/vulkan_raii.hpp>

TEST_CASE("Push (Move)")
{
	auto linked =
		vulkan::LinkedStruct(vk::DeviceCreateInfo())
			.push(vk::PhysicalDeviceFeatures2())
			.push(vk::PhysicalDeviceVulkan11Features());

	const auto primary = linked.get();
	REQUIRE(primary.sType == vk::StructureType::eDeviceCreateInfo);
	REQUIRE(primary.pNext != nullptr);

	const auto features2 = reinterpret_cast<const vk::PhysicalDeviceFeatures2*>(primary.pNext);
	REQUIRE(features2->sType == vk::StructureType::ePhysicalDeviceFeatures2);
	REQUIRE(features2->pNext != nullptr);

	const auto features11 = reinterpret_cast<const vk::PhysicalDeviceVulkan11Features*>(features2->pNext);
	REQUIRE(features11->sType == vk::StructureType::ePhysicalDeviceVulkan11Features);
	REQUIRE(features11->pNext == nullptr);
}

TEST_CASE("Push (Non-move)")
{
	auto linked = vulkan::LinkedStruct(vk::DeviceCreateInfo());
	linked.push(vk::PhysicalDeviceFeatures2()).push(vk::PhysicalDeviceVulkan11Features());

	const auto primary = linked.get();
	REQUIRE(primary.sType == vk::StructureType::eDeviceCreateInfo);
	REQUIRE(primary.pNext != nullptr);

	const auto features2 = reinterpret_cast<const vk::PhysicalDeviceFeatures2*>(primary.pNext);
	REQUIRE(features2->sType == vk::StructureType::ePhysicalDeviceFeatures2);
	REQUIRE(features2->pNext != nullptr);

	const auto features11 = reinterpret_cast<const vk::PhysicalDeviceVulkan11Features*>(features2->pNext);
	REQUIRE(features11->sType == vk::StructureType::ePhysicalDeviceVulkan11Features);
	REQUIRE(features11->pNext == nullptr);
}

TEST_CASE("Pop")
{
	SUBCASE("Pop all")
	{
		auto linked =
			vulkan::LinkedStruct(vk::DeviceCreateInfo())
				.push(vk::PhysicalDeviceFeatures2())
				.push(vk::PhysicalDeviceVulkan11Features());

		REQUIRE(linked.try_pop());
		REQUIRE(linked.try_pop());
		REQUIRE_FALSE(linked.try_pop());

		const auto primary = linked.get();
		REQUIRE(primary.sType == vk::StructureType::eDeviceCreateInfo);
		REQUIRE(primary.pNext == nullptr);
	}

	SUBCASE("Pop some")
	{
		auto linked =
			vulkan::LinkedStruct(vk::DeviceCreateInfo())
				.push(vk::PhysicalDeviceFeatures2())
				.push(vk::PhysicalDeviceVulkan11Features());

		REQUIRE(linked.try_pop());

		const auto primary = linked.get();
		REQUIRE(primary.sType == vk::StructureType::eDeviceCreateInfo);
		REQUIRE(primary.pNext != nullptr);

		const auto features2 = reinterpret_cast<const vk::PhysicalDeviceFeatures2*>(primary.pNext);
		REQUIRE(features2->sType == vk::StructureType::ePhysicalDeviceFeatures2);
		REQUIRE(features2->pNext == nullptr);
	}
}

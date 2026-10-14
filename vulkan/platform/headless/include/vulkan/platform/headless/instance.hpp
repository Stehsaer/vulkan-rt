#pragma once

#include "common/container/error.hpp"
#include "vulkan/common/context.hpp"
#include "vulkan/platform/headless/library.hpp"

#include <expected>
#include <memory>
#include <utility>
#include <vulkan/vulkan_raii.hpp>

namespace vulkan
{
	///
	/// @brief Headless instance for off-screen rendering
	///
	class HeadlessInstance
	{
	  public:

		///
		/// @brief Create a headless instance
		///
		/// @param instance_property Instance property option
		/// @param instance_feature Instance feature option
		/// @return Headless instance or error
		///
		[[nodiscard]]
		static std::expected<HeadlessInstance, Error> create(
			const InstanceProperty& instance_property,
			const InstanceFeature& instance_feature
		) noexcept;

	  private:

		VulkanLibrary vulkan_loader;
		vk::raii::Context context;
		std::unique_ptr<vk::raii::Instance> instance;

		explicit HeadlessInstance(
			VulkanLibrary vulkan_loader,
			vk::raii::Context context,
			vk::raii::Instance instance
		) :
			vulkan_loader(std::move(vulkan_loader)),
			context(std::move(context)),
			instance(std::make_unique<vk::raii::Instance>(std::move(instance)))
		{}

		struct VisitProxy
		{
			const vk::raii::Instance& instance;

			auto operator->() const noexcept { return this; }
		};

	  public:

		VisitProxy operator->() const noexcept { return {.instance = *instance}; }

		HeadlessInstance(const HeadlessInstance&) = delete;
		HeadlessInstance(HeadlessInstance&&) = default;
		HeadlessInstance& operator=(const HeadlessInstance&) = delete;
		HeadlessInstance& operator=(HeadlessInstance&&) = default;
	};
}

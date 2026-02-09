#include "descriptor-layouts.hpp"

namespace
{
	constexpr auto bindings = std::to_array<vk::DescriptorSetLayoutBinding>({
		{.binding = 0,
		 .descriptorType = vk::DescriptorType::eCombinedImageSampler,
		 .descriptorCount = 1,
		 .stageFlags = vk::ShaderStageFlagBits::eFragment}
	});
}

std::expected<DescriptorLayouts, Error> DescriptorLayouts::create(const vk::raii::Device& device) noexcept
{
	const auto create_info = vk::DescriptorSetLayoutCreateInfo{}.setBindings(bindings);

	auto descriptor_layout_result =
		device.createDescriptorSetLayout(create_info).transform_error(Error::from<vk::Result>());
	if (!descriptor_layout_result)
		return descriptor_layout_result.error().forward("Create descriptor set layout failed");
	auto descriptor_layout = std ::move(*descriptor_layout_result);

	return DescriptorLayouts{.main_layout = std::move(descriptor_layout)};
}

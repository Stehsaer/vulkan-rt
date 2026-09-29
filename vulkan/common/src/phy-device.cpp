#include "vulkan/common/trace/phy-device.hpp"
#include "common/container/debug-trace.hpp"
#include "common/container/debug-value.hpp"
#include "common/container/error.hpp"

#include <ranges>
#include <utility>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>
#include <vulkan/vulkan_to_string.hpp>

namespace vulkan
{
	debug::Trace trace_phy_device(const vk::raii::PhysicalDevice& phy_device) noexcept
	{
		using debug::operator""_key;
		using namespace debug::value;

		const auto properties = phy_device.getProperties();

		debug::Trace properties_trace = {
			"vendor-id"_key = Number(properties.vendorID),
			"device-id"_key = Number(properties.deviceID),
			"device-name"_key = String(properties.deviceName.data()),
			"device-type"_key = String(vk::to_string(properties.deviceType)),
			"driver-version"_key = Number(properties.driverVersion),
			"api-version"_key = Number(properties.apiVersion),
		};

		const auto& limits = properties.limits;

		debug::Trace limits_trace = {
			"maxImageDimension1D"_key = Number(limits.maxImageDimension1D),
			"maxImageDimension2D"_key = Number(limits.maxImageDimension2D),
			"maxImageDimension3D"_key = Number(limits.maxImageDimension3D),
			"maxImageDimensionCube"_key = Number(limits.maxImageDimensionCube),
			"maxImageArrayLayers"_key = Number(limits.maxImageArrayLayers),
			"maxTexelBufferElements"_key = Number(limits.maxTexelBufferElements),
			"maxUniformBufferRange"_key = Number(limits.maxUniformBufferRange),
			"maxStorageBufferRange"_key = Number(limits.maxStorageBufferRange),
			"maxPushConstantsSize"_key = Number(limits.maxPushConstantsSize),
			"maxMemoryAllocationCount"_key = Number(limits.maxMemoryAllocationCount),
			"maxSamplerAllocationCount"_key = Number(limits.maxSamplerAllocationCount),
			"bufferImageGranularity"_key = Number(limits.bufferImageGranularity),
			"sparseAddressSpaceSize"_key = Number(limits.sparseAddressSpaceSize),
			"maxBoundDescriptorSets"_key = Number(limits.maxBoundDescriptorSets),
			"maxPerStageDescriptorSamplers"_key = Number(limits.maxPerStageDescriptorSamplers),
			"maxPerStageDescriptorUniformBuffers"_key = Number(limits.maxPerStageDescriptorUniformBuffers),
			"maxPerStageDescriptorStorageBuffers"_key = Number(limits.maxPerStageDescriptorStorageBuffers),
			"maxPerStageDescriptorSampledImages"_key = Number(limits.maxPerStageDescriptorSampledImages),
			"maxPerStageDescriptorStorageImages"_key = Number(limits.maxPerStageDescriptorStorageImages),
			"maxPerStageDescriptorInputAttachments"_key =
				Number(limits.maxPerStageDescriptorInputAttachments),
			"maxPerStageResources"_key = Number(limits.maxPerStageResources),
			"maxDescriptorSetSamplers"_key = Number(limits.maxDescriptorSetSamplers),
			"maxDescriptorSetUniformBuffers"_key = Number(limits.maxDescriptorSetUniformBuffers),
			"maxDescriptorSetUniformBuffersDynamic"_key =
				Number(limits.maxDescriptorSetUniformBuffersDynamic),
			"maxDescriptorSetStorageBuffers"_key = Number(limits.maxDescriptorSetStorageBuffers),
			"maxDescriptorSetStorageBuffersDynamic"_key =
				Number(limits.maxDescriptorSetStorageBuffersDynamic),
			"maxDescriptorSetSampledImages"_key = Number(limits.maxDescriptorSetSampledImages),
			"maxDescriptorSetStorageImages"_key = Number(limits.maxDescriptorSetStorageImages),
			"maxDescriptorSetInputAttachments"_key = Number(limits.maxDescriptorSetInputAttachments),
			"maxVertexInputAttributes"_key = Number(limits.maxVertexInputAttributes),
			"maxVertexInputBindings"_key = Number(limits.maxVertexInputBindings),
			"maxVertexInputAttributeOffset"_key = Number(limits.maxVertexInputAttributeOffset),
			"maxVertexInputBindingStride"_key = Number(limits.maxVertexInputBindingStride),
			"maxVertexOutputComponents"_key = Number(limits.maxVertexOutputComponents),
			"maxTessellationGenerationLevel"_key = Number(limits.maxTessellationGenerationLevel),
			"maxTessellationPatchSize"_key = Number(limits.maxTessellationPatchSize),
			"maxTessellationControlPerVertexInputComponents"_key =
				Number(limits.maxTessellationControlPerVertexInputComponents),
			"maxTessellationControlPerVertexOutputComponents"_key =
				Number(limits.maxTessellationControlPerVertexOutputComponents),
			"maxTessellationControlPerPatchOutputComponents"_key =
				Number(limits.maxTessellationControlPerPatchOutputComponents),
			"maxTessellationControlTotalOutputComponents"_key =
				Number(limits.maxTessellationControlTotalOutputComponents),
			"maxTessellationEvaluationInputComponents"_key =
				Number(limits.maxTessellationEvaluationInputComponents),
			"maxTessellationEvaluationOutputComponents"_key =
				Number(limits.maxTessellationEvaluationOutputComponents),
			"maxGeometryShaderInvocations"_key = Number(limits.maxGeometryShaderInvocations),
			"maxGeometryInputComponents"_key = Number(limits.maxGeometryInputComponents),
			"maxGeometryOutputComponents"_key = Number(limits.maxGeometryOutputComponents),
			"maxGeometryOutputVertices"_key = Number(limits.maxGeometryOutputVertices),
			"maxGeometryTotalOutputComponents"_key = Number(limits.maxGeometryTotalOutputComponents),
			"maxFragmentInputComponents"_key = Number(limits.maxFragmentInputComponents),
			"maxFragmentOutputAttachments"_key = Number(limits.maxFragmentOutputAttachments),
			"maxFragmentDualSrcAttachments"_key = Number(limits.maxFragmentDualSrcAttachments),
			"maxFragmentCombinedOutputResources"_key = Number(limits.maxFragmentCombinedOutputResources),
			"maxComputeSharedMemorySize"_key = Number(limits.maxComputeSharedMemorySize),
			"maxComputeWorkGroupCount"_key =
				{
                   "x"_key = Number(limits.maxComputeWorkGroupCount[0]),
                   "y"_key = Number(limits.maxComputeWorkGroupCount[1]),
                   "z"_key = Number(limits.maxComputeWorkGroupCount[2]),
				   },
			"maxComputeWorkGroupInvocations"_key = Number(limits.maxComputeWorkGroupInvocations),
			"maxComputeWorkGroupSize"_key =
				{
                   "x"_key = Number(limits.maxComputeWorkGroupSize[0]),
                   "y"_key = Number(limits.maxComputeWorkGroupSize[1]),
                   "z"_key = Number(limits.maxComputeWorkGroupSize[2]),
				   },
			"subPixelPrecisionBits"_key = Number(limits.subPixelPrecisionBits),
			"subTexelPrecisionBits"_key = Number(limits.subTexelPrecisionBits),
			"mipmapPrecisionBits"_key = Number(limits.mipmapPrecisionBits),
			"maxDrawIndexedIndexValue"_key = Number(limits.maxDrawIndexedIndexValue),
			"maxDrawIndirectCount"_key = Number(limits.maxDrawIndirectCount),
			"maxSamplerLodBias"_key = Number(limits.maxSamplerLodBias),
			"maxSamplerAnisotropy"_key = Number(limits.maxSamplerAnisotropy),
			"maxViewports"_key = Number(limits.maxViewports),
			"maxViewportDimensions"_key =
				{
                   "x"_key = Number(limits.maxViewportDimensions[0]),
                   "y"_key = Number(limits.maxViewportDimensions[1]),
				   },
			"viewportBoundsRange"_key =
				{
                   "x"_key = Number(limits.viewportBoundsRange[0]),
                   "y"_key = Number(limits.viewportBoundsRange[1]),
				   },
			"viewportSubPixelBits"_key = Number(limits.viewportSubPixelBits),
			"minMemoryMapAlignment"_key = Number(limits.minMemoryMapAlignment),
			"minTexelBufferOffsetAlignment"_key = Number(limits.minTexelBufferOffsetAlignment),
			"minUniformBufferOffsetAlignment"_key = Number(limits.minUniformBufferOffsetAlignment),
			"minStorageBufferOffsetAlignment"_key = Number(limits.minStorageBufferOffsetAlignment),
			"minTexelOffset"_key = Number(limits.minTexelOffset),
			"maxTexelOffset"_key = Number(limits.maxTexelOffset),
			"minTexelGatherOffset"_key = Number(limits.minTexelGatherOffset),
			"maxTexelGatherOffset"_key = Number(limits.maxTexelGatherOffset),
			"minInterpolationOffset"_key = Number(limits.minInterpolationOffset),
			"maxInterpolationOffset"_key = Number(limits.maxInterpolationOffset),
			"subPixelInterpolationOffsetBits"_key = Number(limits.subPixelInterpolationOffsetBits),
			"maxFramebufferWidth"_key = Number(limits.maxFramebufferWidth),
			"maxFramebufferHeight"_key = Number(limits.maxFramebufferHeight),
			"maxFramebufferLayers"_key = Number(limits.maxFramebufferLayers),
			"framebufferColorSampleCounts"_key = String(vk::to_string(limits.framebufferColorSampleCounts)),
			"framebufferDepthSampleCounts"_key = String(vk::to_string(limits.framebufferDepthSampleCounts)),
			"framebufferStencilSampleCounts"_key =
				String(vk::to_string(limits.framebufferStencilSampleCounts)),
			"framebufferNoAttachmentsSampleCounts"_key =
				String(vk::to_string(limits.framebufferNoAttachmentsSampleCounts)),
			"maxColorAttachments"_key = Number(limits.maxColorAttachments),
			"sampledImageColorSampleCounts"_key = String(vk::to_string(limits.sampledImageColorSampleCounts)),
			"sampledImageIntegerSampleCounts"_key =
				String(vk::to_string(limits.sampledImageIntegerSampleCounts)),
			"sampledImageDepthSampleCounts"_key = String(vk::to_string(limits.sampledImageDepthSampleCounts)),
			"sampledImageStencilSampleCounts"_key =
				String(vk::to_string(limits.sampledImageStencilSampleCounts)),
			"storageImageSampleCounts"_key = String(vk::to_string(limits.storageImageSampleCounts)),
			"maxSampleMaskWords"_key = Number(limits.maxSampleMaskWords),
			"timestampComputeAndGraphics"_key = Number(limits.timestampComputeAndGraphics),
			"timestampPeriod"_key = Number(limits.timestampPeriod),
			"maxClipDistances"_key = Number(limits.maxClipDistances),
			"maxCullDistances"_key = Number(limits.maxCullDistances),
			"maxCombinedClipAndCullDistances"_key = Number(limits.maxCombinedClipAndCullDistances),
			"discreteQueuePriorities"_key = Number(limits.discreteQueuePriorities),
			"pointSizeRange"_key =
				{
                   "min"_key = Number(limits.pointSizeRange[0]),
                   "max"_key = Number(limits.pointSizeRange[1]),
				   },
			"lineWidthRange"_key =
				{
                   "min"_key = Number(limits.lineWidthRange[0]),
                   "max"_key = Number(limits.lineWidthRange[1]),
				   },
			"pointSizeGranularity"_key = Number(limits.pointSizeGranularity),
			"lineWidthGranularity"_key = Number(limits.lineWidthGranularity),
			"strictLines"_key = Number(limits.strictLines),
			"standardSampleLocations"_key = Number(limits.standardSampleLocations),
			"optimalBufferCopyOffsetAlignment"_key = Number(limits.optimalBufferCopyOffsetAlignment),
			"optimalBufferCopyRowPitchAlignment"_key = Number(limits.optimalBufferCopyRowPitchAlignment),
			"nonCoherentAtomSize"_key = Number(limits.nonCoherentAtomSize),
		};

		const auto features2 = phy_device.getFeatures2<
			vk::PhysicalDeviceFeatures2,
			vk::PhysicalDeviceVulkan11Features,
			vk::PhysicalDeviceVulkan12Features,
			vk::PhysicalDeviceVulkan13Features
		>();

		const auto features_10 = features2.get<vk::PhysicalDeviceFeatures2>().features;
		const auto features_11 = features2.get<vk::PhysicalDeviceVulkan11Features>();
		const auto features_12 = features2.get<vk::PhysicalDeviceVulkan12Features>();
		const auto features_13 = features2.get<vk::PhysicalDeviceVulkan13Features>();

		debug::Trace features_10_trace = {
			"robustBufferAccess"_key = Boolean(features_10.robustBufferAccess),
			"fullDrawIndexUint32"_key = Boolean(features_10.fullDrawIndexUint32),
			"imageCubeArray"_key = Boolean(features_10.imageCubeArray),
			"independentBlend"_key = Boolean(features_10.independentBlend),
			"geometryShader"_key = Boolean(features_10.geometryShader),
			"tessellationShader"_key = Boolean(features_10.tessellationShader),
			"sampleRateShading"_key = Boolean(features_10.sampleRateShading),
			"dualSrcBlend"_key = Boolean(features_10.dualSrcBlend),
			"logicOp"_key = Boolean(features_10.logicOp),
			"multiDrawIndirect"_key = Boolean(features_10.multiDrawIndirect),
			"drawIndirectFirstInstance"_key = Boolean(features_10.drawIndirectFirstInstance),
			"depthClamp"_key = Boolean(features_10.depthClamp),
			"depthBiasClamp"_key = Boolean(features_10.depthBiasClamp),
			"fillModeNonSolid"_key = Boolean(features_10.fillModeNonSolid),
			"depthBounds"_key = Boolean(features_10.depthBounds),
			"wideLines"_key = Boolean(features_10.wideLines),
			"largePoints"_key = Boolean(features_10.largePoints),
			"alphaToOne"_key = Boolean(features_10.alphaToOne),
			"multiViewport"_key = Boolean(features_10.multiViewport),
			"samplerAnisotropy"_key = Boolean(features_10.samplerAnisotropy),
			"textureCompressionETC2"_key = Boolean(features_10.textureCompressionETC2),
			"textureCompressionASTC_LDR"_key = Boolean(features_10.textureCompressionASTC_LDR),
			"textureCompressionBC"_key = Boolean(features_10.textureCompressionBC),
			"occlusionQueryPrecise"_key = Boolean(features_10.occlusionQueryPrecise),
			"pipelineStatisticsQuery"_key = Boolean(features_10.pipelineStatisticsQuery),
			"vertexPipelineStoresAndAtomics"_key = Boolean(features_10.vertexPipelineStoresAndAtomics),
			"fragmentStoresAndAtomics"_key = Boolean(features_10.fragmentStoresAndAtomics),
			"shaderTessellationAndGeometryPointSize"_key =
				Boolean(features_10.shaderTessellationAndGeometryPointSize),
			"shaderImageGatherExtended"_key = Boolean(features_10.shaderImageGatherExtended),
			"shaderStorageImageExtendedFormats"_key = Boolean(features_10.shaderStorageImageExtendedFormats),
			"shaderStorageImageMultisample"_key = Boolean(features_10.shaderStorageImageMultisample),
			"shaderStorageImageReadWithoutFormat"_key =
				Boolean(features_10.shaderStorageImageReadWithoutFormat),
			"shaderStorageImageWriteWithoutFormat"_key =
				Boolean(features_10.shaderStorageImageWriteWithoutFormat),
			"shaderUniformBufferArrayDynamicIndexing"_key =
				Boolean(features_10.shaderUniformBufferArrayDynamicIndexing),
			"shaderSampledImageArrayDynamicIndexing"_key =
				Boolean(features_10.shaderSampledImageArrayDynamicIndexing),
			"shaderStorageBufferArrayDynamicIndexing"_key =
				Boolean(features_10.shaderStorageBufferArrayDynamicIndexing),
			"shaderStorageImageArrayDynamicIndexing"_key =
				Boolean(features_10.shaderStorageImageArrayDynamicIndexing),
			"shaderClipDistance"_key = Boolean(features_10.shaderClipDistance),
			"shaderCullDistance"_key = Boolean(features_10.shaderCullDistance),
			"shaderFloat64"_key = Boolean(features_10.shaderFloat64),
			"shaderInt64"_key = Boolean(features_10.shaderInt64),
			"shaderInt16"_key = Boolean(features_10.shaderInt16),
			"shaderResourceResidency"_key = Boolean(features_10.shaderResourceResidency),
			"shaderResourceMinLod"_key = Boolean(features_10.shaderResourceMinLod),
			"sparseBinding"_key = Boolean(features_10.sparseBinding),
			"sparseResidencyBuffer"_key = Boolean(features_10.sparseResidencyBuffer),
			"sparseResidencyImage2D"_key = Boolean(features_10.sparseResidencyImage2D),
			"sparseResidencyImage3D"_key = Boolean(features_10.sparseResidencyImage3D),
			"sparseResidency2Samples"_key = Boolean(features_10.sparseResidency2Samples),
			"sparseResidency4Samples"_key = Boolean(features_10.sparseResidency4Samples),
			"sparseResidency8Samples"_key = Boolean(features_10.sparseResidency8Samples),
			"sparseResidency16Samples"_key = Boolean(features_10.sparseResidency16Samples),
			"sparseResidencyAliased"_key = Boolean(features_10.sparseResidencyAliased),
			"variableMultisampleRate"_key = Boolean(features_10.variableMultisampleRate),
			"inheritedQueries"_key = Boolean(features_10.inheritedQueries),
		};

		debug::Trace features_11_trace = {
			"storageBuffer16BitAccess"_key = Boolean(features_11.storageBuffer16BitAccess),
			"uniformAndStorageBuffer16BitAccess"_key =
				Boolean(features_11.uniformAndStorageBuffer16BitAccess),
			"storagePushConstant16"_key = Boolean(features_11.storagePushConstant16),
			"storageInputOutput16"_key = Boolean(features_11.storageInputOutput16),
			"multiview"_key = Boolean(features_11.multiview),
			"multiviewGeometryShader"_key = Boolean(features_11.multiviewGeometryShader),
			"multiviewTessellationShader"_key = Boolean(features_11.multiviewTessellationShader),
			"variablePointersStorageBuffer"_key = Boolean(features_11.variablePointersStorageBuffer),
			"variablePointers"_key = Boolean(features_11.variablePointers),
			"protectedMemory"_key = Boolean(features_11.protectedMemory),
			"samplerYcbcrConversion"_key = Boolean(features_11.samplerYcbcrConversion),
			"shaderDrawParameters"_key = Boolean(features_11.shaderDrawParameters),
		};

		debug::Trace features_12_trace = {
			"samplerMirrorClampToEdge"_key = Boolean(features_12.samplerMirrorClampToEdge),
			"drawIndirectCount"_key = Boolean(features_12.drawIndirectCount),
			"storageBuffer8BitAccess"_key = Boolean(features_12.storageBuffer8BitAccess),
			"uniformAndStorageBuffer8BitAccess"_key = Boolean(features_12.uniformAndStorageBuffer8BitAccess),
			"storagePushConstant8"_key = Boolean(features_12.storagePushConstant8),
			"shaderBufferInt64Atomics"_key = Boolean(features_12.shaderBufferInt64Atomics),
			"shaderSharedInt64Atomics"_key = Boolean(features_12.shaderSharedInt64Atomics),
			"shaderFloat16"_key = Boolean(features_12.shaderFloat16),
			"shaderInt8"_key = Boolean(features_12.shaderInt8),
			"descriptorIndexing"_key = Boolean(features_12.descriptorIndexing),
			"shaderInputAttachmentArrayDynamicIndexing"_key =
				Boolean(features_12.shaderInputAttachmentArrayDynamicIndexing),
			"shaderUniformTexelBufferArrayDynamicIndexing"_key =
				Boolean(features_12.shaderUniformTexelBufferArrayDynamicIndexing),
			"shaderStorageTexelBufferArrayDynamicIndexing"_key =
				Boolean(features_12.shaderStorageTexelBufferArrayDynamicIndexing),
			"shaderUniformBufferArrayNonUniformIndexing"_key =
				Boolean(features_12.shaderUniformBufferArrayNonUniformIndexing),
			"shaderSampledImageArrayNonUniformIndexing"_key =
				Boolean(features_12.shaderSampledImageArrayNonUniformIndexing),
			"shaderStorageBufferArrayNonUniformIndexing"_key =
				Boolean(features_12.shaderStorageBufferArrayNonUniformIndexing),
			"shaderStorageImageArrayNonUniformIndexing"_key =
				Boolean(features_12.shaderStorageImageArrayNonUniformIndexing),
			"shaderInputAttachmentArrayNonUniformIndexing"_key =
				Boolean(features_12.shaderInputAttachmentArrayNonUniformIndexing),
			"shaderUniformTexelBufferArrayNonUniformIndexing"_key =
				Boolean(features_12.shaderUniformTexelBufferArrayNonUniformIndexing),
			"shaderStorageTexelBufferArrayNonUniformIndexing"_key =
				Boolean(features_12.shaderStorageTexelBufferArrayNonUniformIndexing),
			"descriptorBindingUniformBufferUpdateAfterBind"_key =
				Boolean(features_12.descriptorBindingUniformBufferUpdateAfterBind),
			"descriptorBindingSampledImageUpdateAfterBind"_key =
				Boolean(features_12.descriptorBindingSampledImageUpdateAfterBind),
			"descriptorBindingStorageImageUpdateAfterBind"_key =
				Boolean(features_12.descriptorBindingStorageImageUpdateAfterBind),
			"descriptorBindingStorageBufferUpdateAfterBind"_key =
				Boolean(features_12.descriptorBindingStorageBufferUpdateAfterBind),
			"descriptorBindingUniformTexelBufferUpdateAfterBind"_key =
				Boolean(features_12.descriptorBindingUniformTexelBufferUpdateAfterBind),
			"descriptorBindingStorageTexelBufferUpdateAfterBind"_key =
				Boolean(features_12.descriptorBindingStorageTexelBufferUpdateAfterBind),
			"descriptorBindingUpdateUnusedWhilePending"_key =
				Boolean(features_12.descriptorBindingUpdateUnusedWhilePending),
			"descriptorBindingPartiallyBound"_key = Boolean(features_12.descriptorBindingPartiallyBound),
			"descriptorBindingVariableDescriptorCount"_key =
				Boolean(features_12.descriptorBindingVariableDescriptorCount),
			"runtimeDescriptorArray"_key = Boolean(features_12.runtimeDescriptorArray),
			"samplerFilterMinmax"_key = Boolean(features_12.samplerFilterMinmax),
			"scalarBlockLayout"_key = Boolean(features_12.scalarBlockLayout),
			"imagelessFramebuffer"_key = Boolean(features_12.imagelessFramebuffer),
			"uniformBufferStandardLayout"_key = Boolean(features_12.uniformBufferStandardLayout),
			"shaderSubgroupExtendedTypes"_key = Boolean(features_12.shaderSubgroupExtendedTypes),
			"separateDepthStencilLayouts"_key = Boolean(features_12.separateDepthStencilLayouts),
			"hostQueryReset"_key = Boolean(features_12.hostQueryReset),
			"timelineSemaphore"_key = Boolean(features_12.timelineSemaphore),
			"bufferDeviceAddress"_key = Boolean(features_12.bufferDeviceAddress),
			"bufferDeviceAddressCaptureReplay"_key = Boolean(features_12.bufferDeviceAddressCaptureReplay),
			"bufferDeviceAddressMultiDevice"_key = Boolean(features_12.bufferDeviceAddressMultiDevice),
			"vulkanMemoryModel"_key = Boolean(features_12.vulkanMemoryModel),
			"vulkanMemoryModelDeviceScope"_key = Boolean(features_12.vulkanMemoryModelDeviceScope),
			"vulkanMemoryModelAvailabilityVisibilityChains"_key =
				Boolean(features_12.vulkanMemoryModelAvailabilityVisibilityChains),
			"shaderOutputViewportIndex"_key = Boolean(features_12.shaderOutputViewportIndex),
			"shaderOutputLayer"_key = Boolean(features_12.shaderOutputLayer),
			"subgroupBroadcastDynamicId"_key = Boolean(features_12.subgroupBroadcastDynamicId),
		};

		debug::Trace features_13_trace = {
			"robustImageAccess"_key = Boolean(features_13.robustImageAccess),
			"inlineUniformBlock"_key = Boolean(features_13.inlineUniformBlock),
			"descriptorBindingInlineUniformBlockUpdateAfterBind"_key =
				Boolean(features_13.descriptorBindingInlineUniformBlockUpdateAfterBind),
			"pipelineCreationCacheControl"_key = Boolean(features_13.pipelineCreationCacheControl),
			"privateData"_key = Boolean(features_13.privateData),
			"shaderDemoteToHelperInvocation"_key = Boolean(features_13.shaderDemoteToHelperInvocation),
			"shaderTerminateInvocation"_key = Boolean(features_13.shaderTerminateInvocation),
			"subgroupSizeControl"_key = Boolean(features_13.subgroupSizeControl),
			"computeFullSubgroups"_key = Boolean(features_13.computeFullSubgroups),
			"synchronization2"_key = Boolean(features_13.synchronization2),
			"textureCompressionASTC_HDR"_key = Boolean(features_13.textureCompressionASTC_HDR),
			"shaderZeroInitializeWorkgroupMemory"_key =
				Boolean(features_13.shaderZeroInitializeWorkgroupMemory),
			"dynamicRendering"_key = Boolean(features_13.dynamicRendering),
			"shaderIntegerDotProduct"_key = Boolean(features_13.shaderIntegerDotProduct),
			"maintenance4"_key = Boolean(features_13.maintenance4),
		};

		const auto extensions_result = phy_device.enumerateDeviceLayerProperties();
		auto extensions_trace = [&extensions_result] -> debug::Trace {
			if (!extensions_result)
				return {"error"_key = debug::value::Error(::Error::from(extensions_result))};

			const auto& extensions = *extensions_result;

			return {
				"count"_key = Number(extensions.size()),
				"extensions"_key =
					extensions | std::views::transform([](const vk::LayerProperties& prop) {
						return String(prop.layerName.data());
					})
			};
		}();

		return {
			"properties"_key = std::move(properties_trace),
			"limits"_key = std::move(limits_trace),
			"extensions"_key = std::move(extensions_trace),
			"features-vk1.0"_key = std::move(features_10_trace),
			"features-vk1.1"_key = std::move(features_11_trace),
			"features-vk1.2"_key = std::move(features_12_trace),
			"features-vk1.3"_key = std::move(features_13_trace),
		};
	}
}

// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include "rhi_vulkan.h"

#if defined(BONES_VULKAN)

#include <vulkan/vulkan.h>

#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <vector>

namespace bones::rhi
{
namespace
{

void LogVulkan(const char* message, VkResult result)
{
	std::fprintf(stderr, "[Vulkan] %s (VkResult %d)\n", message, static_cast<int>(result));
}

bool GetTextureFormat(Format format, VkFormat& outFormat, VkImageAspectFlags& outAspect)
{
	switch (format)
	{
	case Format::R8_UNorm:
		outFormat = VK_FORMAT_R8_UNORM;
		outAspect = VK_IMAGE_ASPECT_COLOR_BIT;
		return true;
	case Format::R8G8B8A8_UNorm:
		outFormat = VK_FORMAT_R8G8B8A8_UNORM;
		outAspect = VK_IMAGE_ASPECT_COLOR_BIT;
		return true;
	case Format::R8G8B8A8_SRGB:
		outFormat = VK_FORMAT_R8G8B8A8_SRGB;
		outAspect = VK_IMAGE_ASPECT_COLOR_BIT;
		return true;
	case Format::R32_Float:
		outFormat = VK_FORMAT_R32_SFLOAT;
		outAspect = VK_IMAGE_ASPECT_COLOR_BIT;
		return true;
	case Format::D24_UNorm_S8_UInt:
		outFormat = VK_FORMAT_D24_UNORM_S8_UINT;
		outAspect = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
		return true;
	case Format::D32_Float:
		outFormat = VK_FORMAT_D32_SFLOAT;
		outAspect = VK_IMAGE_ASPECT_DEPTH_BIT;
		return true;
	default:
		return false;
	}
}

bool GetVertexFormat(Format format, VkFormat& outFormat)
{
	switch (format)
	{
	case Format::R32_Float:
		outFormat = VK_FORMAT_R32_SFLOAT;
		return true;
	case Format::R32G32_Float:
		outFormat = VK_FORMAT_R32G32_SFLOAT;
		return true;
	case Format::R32G32B32_Float:
		outFormat = VK_FORMAT_R32G32B32_SFLOAT;
		return true;
	case Format::R32G32B32A32_Float:
		outFormat = VK_FORMAT_R32G32B32A32_SFLOAT;
		return true;
	default:
		return false;
	}
}

uint32_t FindMemoryType(VkPhysicalDevice physicalDevice, uint32_t typeBits, VkMemoryPropertyFlags properties)
{
	VkPhysicalDeviceMemoryProperties memoryProperties{};
	vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

	for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
	{
		if ((typeBits & (1u << i)) != 0 && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			return i;
	}
	return UINT32_MAX;
}

void TransitionImage(VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspect,
	VkImageLayout oldLayout, VkImageLayout newLayout,
	VkAccessFlags srcAccess, VkAccessFlags dstAccess)
{
	VkImageMemoryBarrier barrier{};
	barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	barrier.oldLayout = oldLayout;
	barrier.newLayout = newLayout;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange.aspectMask = aspect;
	barrier.subresourceRange.baseMipLevel = 0;
	barrier.subresourceRange.levelCount = 1;
	barrier.subresourceRange.baseArrayLayer = 0;
	barrier.subresourceRange.layerCount = 1;
	barrier.srcAccessMask = srcAccess;
	barrier.dstAccessMask = dstAccess;

	vkCmdPipelineBarrier(commandBuffer,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		0, 0, nullptr, 0, nullptr, 1, &barrier);
}

VkBufferUsageFlags GetBufferUsage(BufferUsage usage)
{
	switch (usage)
	{
	case BufferUsage::Index:
		return VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	case BufferUsage::Uniform:
		return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
	case BufferUsage::Vertex:
	default:
		return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	}
}

bool CreateHostBuffer(VkPhysicalDevice physicalDevice, VkDevice device, VkDeviceSize size,
	VkBufferUsageFlags usage, VkBuffer& buffer, VkDeviceMemory& memory, void*& mapped)
{
	VkBufferCreateInfo bufferInfo{};
	bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferInfo.size = size;
	bufferInfo.usage = usage;
	bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	VkResult result = vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);
	if (result != VK_SUCCESS)
	{
		LogVulkan("vkCreateBuffer failed", result);
		return false;
	}

	VkMemoryRequirements requirements{};
	vkGetBufferMemoryRequirements(device, buffer, &requirements);

	const uint32_t memoryType = FindMemoryType(physicalDevice, requirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
	if (memoryType == UINT32_MAX)
	{
		vkDestroyBuffer(device, buffer, nullptr);
		buffer = VK_NULL_HANDLE;
		return false;
	}

	VkMemoryAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocateInfo.allocationSize = requirements.size;
	allocateInfo.memoryTypeIndex = memoryType;

	result = vkAllocateMemory(device, &allocateInfo, nullptr, &memory);
	if (result != VK_SUCCESS)
	{
		LogVulkan("vkAllocateMemory failed", result);
		vkDestroyBuffer(device, buffer, nullptr);
		buffer = VK_NULL_HANDLE;
		return false;
	}

	result = vkBindBufferMemory(device, buffer, memory, 0);
	if (result != VK_SUCCESS)
	{
		LogVulkan("vkBindBufferMemory failed", result);
		vkDestroyBuffer(device, buffer, nullptr);
		vkFreeMemory(device, memory, nullptr);
		buffer = VK_NULL_HANDLE;
		memory = VK_NULL_HANDLE;
		return false;
	}

	result = vkMapMemory(device, memory, 0, size, 0, &mapped);
	if (result != VK_SUCCESS)
	{
		LogVulkan("vkMapMemory failed", result);
		mapped = nullptr;
	}
	return true;
}

class VkRhiBuffer final : public Buffer
{
public:
	explicit VkRhiBuffer(const BufferDesc& desc) : m_desc(desc) { }

	~VkRhiBuffer() override
	{
		if (m_mapped != nullptr)
			vkUnmapMemory(m_device, m_memory);
		if (m_buffer != VK_NULL_HANDLE)
			vkDestroyBuffer(m_device, m_buffer, nullptr);
		if (m_memory != VK_NULL_HANDLE)
			vkFreeMemory(m_device, m_memory, nullptr);
	}

	bool Create(VkPhysicalDevice physicalDevice, VkDevice device, const void* initialData)
	{
		m_device = device;
		if (m_desc.size == 0)
			return false;

		if (!CreateHostBuffer(physicalDevice, device, m_desc.size, GetBufferUsage(m_desc.usage), m_buffer, m_memory, m_mapped))
			return false;

		if (initialData != nullptr && m_mapped == nullptr)
			return false;
		if (initialData != nullptr)
			std::memcpy(m_mapped, initialData, static_cast<size_t>(m_desc.size));
		return true;
	}

	void Update(const void* data, uint64_t size, uint64_t offset)
	{
		if (m_mapped == nullptr || data == nullptr || size == 0 || offset + size > m_desc.size)
			return;
		std::memcpy(static_cast<uint8_t*>(m_mapped) + offset, data, static_cast<size_t>(size));
	}

	[[nodiscard]] uint64_t GetSize() const override
	{
		return m_desc.size;
	}

	[[nodiscard]] VkBuffer GetBuffer() const
	{
		return m_buffer;
	}

	[[nodiscard]] BufferUsage GetUsage() const
	{
		return m_desc.usage;
	}

private:
	BufferDesc m_desc;
	VkDevice m_device = VK_NULL_HANDLE;
	VkBuffer m_buffer = VK_NULL_HANDLE;
	VkDeviceMemory m_memory = VK_NULL_HANDLE;
	void* m_mapped = nullptr;
};

class VkTexture final : public Texture
{
public:
	VkTexture(VkDevice device, VkPhysicalDevice physicalDevice, const TextureDesc& desc)
		: m_device(device), m_physicalDevice(physicalDevice), m_desc(desc) { }

	~VkTexture() override
	{
		if (m_sampler != VK_NULL_HANDLE)
			vkDestroySampler(m_device, m_sampler, nullptr);
		if (m_view != VK_NULL_HANDLE)
			vkDestroyImageView(m_device, m_view, nullptr);
		if (m_image != VK_NULL_HANDLE)
			vkDestroyImage(m_device, m_image, nullptr);
		if (m_memory != VK_NULL_HANDLE)
			vkFreeMemory(m_device, m_memory, nullptr);
	}

	bool Create()
	{
		VkFormat format = VK_FORMAT_UNDEFINED;
		if (m_desc.width == 0 || m_desc.height == 0 || !GetTextureFormat(m_desc.format, format, m_aspect))
			return false;

		VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
		if (HasTextureUsage(m_desc.usage, TextureUsage::Sampled))
			usage |= VK_IMAGE_USAGE_SAMPLED_BIT;
		if (HasTextureUsage(m_desc.usage, TextureUsage::RenderTarget))
			usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		if (HasTextureUsage(m_desc.usage, TextureUsage::DepthStencil))
			usage |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

		VkImageCreateInfo imageInfo{};
		imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		imageInfo.imageType = VK_IMAGE_TYPE_2D;
		imageInfo.format = format;
		imageInfo.extent.width = m_desc.width;
		imageInfo.extent.height = m_desc.height;
		imageInfo.extent.depth = 1;
		imageInfo.mipLevels = 1;
		imageInfo.arrayLayers = 1;
		imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
		imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
		imageInfo.usage = usage;
		imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		VkResult result = vkCreateImage(m_device, &imageInfo, nullptr, &m_image);
		if (result != VK_SUCCESS)
		{
			LogVulkan("vkCreateImage failed", result);
			return false;
		}

		VkMemoryRequirements requirements{};
		vkGetImageMemoryRequirements(m_device, m_image, &requirements);

		const uint32_t memoryType = FindMemoryType(m_physicalDevice, requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
		if (memoryType == UINT32_MAX)
		{
			vkDestroyImage(m_device, m_image, nullptr);
			m_image = VK_NULL_HANDLE;
			return false;
		}

		VkMemoryAllocateInfo allocateInfo{};
		allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocateInfo.allocationSize = requirements.size;
		allocateInfo.memoryTypeIndex = memoryType;

		result = vkAllocateMemory(m_device, &allocateInfo, nullptr, &m_memory);
		if (result != VK_SUCCESS)
		{
			LogVulkan("vkAllocateMemory failed", result);
			return false;
		}

		result = vkBindImageMemory(m_device, m_image, m_memory, 0);
		if (result != VK_SUCCESS)
		{
			LogVulkan("vkBindImageMemory failed", result);
			return false;
		}

		VkImageViewCreateInfo viewInfo{};
		viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		viewInfo.image = m_image;
		viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		viewInfo.format = format;
		viewInfo.subresourceRange.aspectMask = m_aspect;
		viewInfo.subresourceRange.baseMipLevel = 0;
		viewInfo.subresourceRange.levelCount = 1;
		viewInfo.subresourceRange.baseArrayLayer = 0;
		viewInfo.subresourceRange.layerCount = 1;

		result = vkCreateImageView(m_device, &viewInfo, nullptr, &m_view);
		if (result != VK_SUCCESS)
		{
			LogVulkan("vkCreateImageView failed", result);
			return false;
		}

		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = m_desc.magFilter == Filter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
		samplerInfo.minFilter = m_desc.minFilter == Filter::Nearest ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
		samplerInfo.addressModeU = m_desc.wrap == WrapMode::Repeat ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerInfo.addressModeV = samplerInfo.addressModeU;
		samplerInfo.addressModeW = samplerInfo.addressModeU;
		samplerInfo.anisotropyEnable = VK_FALSE;
		samplerInfo.maxAnisotropy = 1.0f;
		samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		samplerInfo.unnormalizedCoordinates = VK_FALSE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		samplerInfo.mipLodBias = 0.0f;
		samplerInfo.minLod = 0.0f;
		samplerInfo.maxLod = 0.0f;

		result = vkCreateSampler(m_device, &samplerInfo, nullptr, &m_sampler);
		if (result != VK_SUCCESS)
		{
			LogVulkan("vkCreateSampler failed", result);
			return false;
		}

		return true;
	}

	[[nodiscard]] const TextureDesc& GetDesc() const override
	{
		return m_desc;
	}

	[[nodiscard]] VkImage GetImage() const
	{
		return m_image;
	}

	[[nodiscard]] VkImageView GetView() const
	{
		return m_view;
	}

	[[nodiscard]] VkImageAspectFlags GetAspect() const
	{
		return m_aspect;
	}

	VkImageLayout layout = VK_IMAGE_LAYOUT_UNDEFINED;

private:
	VkDevice m_device = VK_NULL_HANDLE;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	TextureDesc m_desc;
	VkImage m_image = VK_NULL_HANDLE;
	VkDeviceMemory m_memory = VK_NULL_HANDLE;
	VkImageView m_view = VK_NULL_HANDLE;
	VkSampler m_sampler = VK_NULL_HANDLE;
	VkImageAspectFlags m_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
};

class VkShader final : public Shader
{
public:
	explicit VkShader(const ShaderDesc& desc) : m_desc(desc) { }

	~VkShader() override
	{
		if (m_module != VK_NULL_HANDLE)
			vkDestroyShaderModule(m_device, m_module, nullptr);
	}

	bool Create(VkDevice device)
	{
		if (m_desc.spirv == nullptr || m_desc.spirvSize == 0 || m_desc.spirvSize % 4 != 0)
			return false;
		if (m_desc.stage != ShaderStage::Vertex && m_desc.stage != ShaderStage::Fragment)
			return false;

		m_device = device;
		if (m_desc.entryPoint != nullptr)
			m_entryPoint = m_desc.entryPoint;

		VkShaderModuleCreateInfo moduleInfo{};
		moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		moduleInfo.codeSize = m_desc.spirvSize;
		moduleInfo.pCode = static_cast<const uint32_t*>(m_desc.spirv);

		VkResult result = vkCreateShaderModule(m_device, &moduleInfo, nullptr, &m_module);
		if (result != VK_SUCCESS)
		{
			LogVulkan("vkCreateShaderModule failed", result);
			m_module = VK_NULL_HANDLE;
			return false;
		}
		return true;
	}

	[[nodiscard]] ShaderStage GetStage() const override
	{
		return m_desc.stage;
	}

	[[nodiscard]] VkShaderModule GetModule() const
	{
		return m_module;
	}

	[[nodiscard]] const char* GetEntryPoint() const
	{
		return m_entryPoint.c_str();
	}

private:
	ShaderDesc m_desc;
	VkDevice m_device = VK_NULL_HANDLE;
	VkShaderModule m_module = VK_NULL_HANDLE;
	std::string m_entryPoint = "main";
};

class VkRhiPipeline final : public Pipeline
{
public:
	explicit VkRhiPipeline(const PipelineDesc& desc) : m_desc(desc) { }

	~VkRhiPipeline() override
	{
		if (m_pipeline != VK_NULL_HANDLE)
			vkDestroyPipeline(m_device, m_pipeline, nullptr);
		if (m_layout != VK_NULL_HANDLE)
			vkDestroyPipelineLayout(m_device, m_layout, nullptr);
	}

	bool Create(VkDevice device)
	{
		if (m_desc.vertexShader == nullptr || m_desc.fragmentShader == nullptr)
			return false;

		m_device = device;
		const VkShaderModule vertexModule = static_cast<VkShader*>(m_desc.vertexShader)->GetModule();
		const VkShaderModule fragmentModule = static_cast<VkShader*>(m_desc.fragmentShader)->GetModule();
		if (vertexModule == VK_NULL_HANDLE || fragmentModule == VK_NULL_HANDLE)
			return false;

		VkFormat colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
		if (m_desc.colorFormat != Format::Undefined && !GetTextureFormat(m_desc.colorFormat, colorFormat, m_aspect))
			return false;
		(void)m_aspect;

		VkFormat depthFormat = VK_FORMAT_UNDEFINED;
		VkImageAspectFlags depthAspect = 0;
		if (m_desc.depthFormat != Format::Undefined && !GetTextureFormat(m_desc.depthFormat, depthFormat, depthAspect))
			return false;

		VkPipelineLayoutCreateInfo layoutInfo{};
		layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		VkResult result = vkCreatePipelineLayout(m_device, &layoutInfo, nullptr, &m_layout);
		if (result != VK_SUCCESS)
		{
			LogVulkan("vkCreatePipelineLayout failed", result);
			return false;
		}

		const VkPipelineShaderStageCreateInfo stages[2] = {
			{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_VERTEX_BIT, vertexModule,
				static_cast<VkShader*>(m_desc.vertexShader)->GetEntryPoint(), nullptr },
			{ VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_FRAGMENT_BIT, fragmentModule,
				static_cast<VkShader*>(m_desc.fragmentShader)->GetEntryPoint(), nullptr },
		};

		VkVertexInputBindingDescription binding{};
		binding.binding = 0;
		binding.stride = m_desc.vertexLayout.stride;
		binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		std::vector<VkVertexInputAttributeDescription> attributes;
		attributes.reserve(m_desc.vertexLayout.attributes.size());
		for (const VertexAttribute& attribute : m_desc.vertexLayout.attributes)
		{
			VkFormat format = VK_FORMAT_UNDEFINED;
			if (!GetVertexFormat(attribute.format, format))
				return false;

			VkVertexInputAttributeDescription attributeDescription{};
			attributeDescription.location = attribute.location;
			attributeDescription.binding = 0;
			attributeDescription.format = format;
			attributeDescription.offset = attribute.offset;
			attributes.push_back(attributeDescription);
		}

		VkPipelineVertexInputStateCreateInfo vertexInput{};
		vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertexInput.vertexBindingDescriptionCount = m_desc.vertexLayout.stride > 0 ? 1 : 0;
		vertexInput.pVertexBindingDescriptions = m_desc.vertexLayout.stride > 0 ? &binding : nullptr;
		vertexInput.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributes.size());
		vertexInput.pVertexAttributeDescriptions = attributes.empty() ? nullptr : attributes.data();

		VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		switch (m_desc.topology)
		{
		case Topology::LineList:
			topology = VK_PRIMITIVE_TOPOLOGY_LINE_LIST;
			break;
		case Topology::PointList:
			topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST;
			break;
		case Topology::TriangleList:
		default:
			topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			break;
		}

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
		inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		inputAssembly.topology = topology;
		inputAssembly.primitiveRestartEnable = VK_FALSE;

		VkPipelineViewportStateCreateInfo viewportState{};
		viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewportState.viewportCount = 1;
		viewportState.scissorCount = 1;

		VkPipelineRasterizationStateCreateInfo rasterization{};
		rasterization.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization.depthClampEnable = VK_FALSE;
		rasterization.rasterizerDiscardEnable = VK_FALSE;
		rasterization.polygonMode = VK_POLYGON_MODE_FILL;
		rasterization.cullMode = m_desc.cullMode == CullMode::Front ? VK_CULL_MODE_FRONT_BIT
			: (m_desc.cullMode == CullMode::Back ? VK_CULL_MODE_BACK_BIT : VK_CULL_MODE_NONE);
		rasterization.frontFace = m_desc.frontFace == FrontFace::Clockwise ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterization.depthBiasEnable = VK_FALSE;
		rasterization.lineWidth = 1.0f;

		VkPipelineMultisampleStateCreateInfo multisample{};
		multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisample.sampleShadingEnable = VK_FALSE;
		multisample.minSampleShading = 1.0f;

		VkPipelineDepthStencilStateCreateInfo depthStencil{};
		depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depthStencil.depthTestEnable = m_desc.depthTest ? VK_TRUE : VK_FALSE;
		depthStencil.depthWriteEnable = m_desc.depthWrite ? VK_TRUE : VK_FALSE;
		depthStencil.depthCompareOp = m_desc.depthCompare == CompareOp::Never ? VK_COMPARE_OP_NEVER
			: m_desc.depthCompare == CompareOp::Equal ? VK_COMPARE_OP_EQUAL
			: m_desc.depthCompare == CompareOp::LessEqual ? VK_COMPARE_OP_LESS_OR_EQUAL
			: m_desc.depthCompare == CompareOp::Greater ? VK_COMPARE_OP_GREATER
			: m_desc.depthCompare == CompareOp::GreaterEqual ? VK_COMPARE_OP_GREATER_OR_EQUAL
			: m_desc.depthCompare == CompareOp::Always ? VK_COMPARE_OP_ALWAYS
			: VK_COMPARE_OP_LESS;
		depthStencil.depthBoundsTestEnable = VK_FALSE;
		depthStencil.stencilTestEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState colorBlendAttachment{};
		colorBlendAttachment.blendEnable = m_desc.blendMode == BlendMode::AlphaBlend ? VK_TRUE : VK_FALSE;
		colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
		colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT
			| VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo colorBlend{};
		colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorBlend.logicOpEnable = VK_FALSE;
		colorBlend.attachmentCount = 1;
		colorBlend.pAttachments = &colorBlendAttachment;

		const VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
		VkPipelineDynamicStateCreateInfo dynamicState{};
		dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamicState.dynamicStateCount = 2;
		dynamicState.pDynamicStates = dynamicStates;

		VkPipelineRenderingCreateInfo renderingInfo{};
		renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		renderingInfo.colorAttachmentCount = 1;
		renderingInfo.pColorAttachmentFormats = &colorFormat;
		renderingInfo.depthAttachmentFormat = depthFormat;

		VkGraphicsPipelineCreateInfo pipelineInfo{};
		pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipelineInfo.pNext = &renderingInfo;
		pipelineInfo.stageCount = 2;
		pipelineInfo.pStages = stages;
		pipelineInfo.pVertexInputState = &vertexInput;
		pipelineInfo.pInputAssemblyState = &inputAssembly;
		pipelineInfo.pViewportState = &viewportState;
		pipelineInfo.pRasterizationState = &rasterization;
		pipelineInfo.pMultisampleState = &multisample;
		pipelineInfo.pDepthStencilState = &depthStencil;
		pipelineInfo.pColorBlendState = &colorBlend;
		pipelineInfo.pDynamicState = &dynamicState;
		pipelineInfo.layout = m_layout;
		pipelineInfo.renderPass = VK_NULL_HANDLE;
		pipelineInfo.subpass = 0;

		result = vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_pipeline);
		if (result != VK_SUCCESS)
		{
			LogVulkan("vkCreateGraphicsPipelines failed", result);
			m_pipeline = VK_NULL_HANDLE;
			return false;
		}
		return true;
	}

	[[nodiscard]] VkPipeline GetPipeline() const
	{
		return m_pipeline;
	}

	[[nodiscard]] VkPipelineLayout GetLayout() const
	{
		return m_layout;
	}

	[[nodiscard]] const PipelineDesc& GetDesc() const
	{
		return m_desc;
	}

private:
	PipelineDesc m_desc;
	VkDevice m_device = VK_NULL_HANDLE;
	VkPipeline m_pipeline = VK_NULL_HANDLE;
	VkPipelineLayout m_layout = VK_NULL_HANDLE;
	VkImageAspectFlags m_aspect = VK_IMAGE_ASPECT_COLOR_BIT;
};

class VulkanDevice final : public Device
{
public:
	VulkanDevice() = default;
	~VulkanDevice() override;

	bool Init();

	[[nodiscard]] Backend GetBackend() const override
	{
		return Backend::Vulkan;
	}

	[[nodiscard]] const char* GetDeviceName() const override
	{
		return m_deviceName.c_str();
	}

	[[nodiscard]] const char* GetApiVersion() const override
	{
		return m_apiVersion.c_str();
	}

	[[nodiscard]] int GetMaxTextureSize() const override
	{
		return m_maxTextureSize;
	}

	std::unique_ptr<Buffer> CreateBuffer(const BufferDesc& desc, const void* initialData) override;
	void UpdateBuffer(Buffer& buffer, const void* data, uint64_t size, uint64_t offset) override;
	std::unique_ptr<Texture> CreateTexture(const TextureDesc& desc, const void* initialData) override;
	std::unique_ptr<Shader> CreateShader(const ShaderDesc& desc) override;
	std::unique_ptr<Pipeline> CreatePipeline(const PipelineDesc& desc) override;
	std::unique_ptr<CommandList> CreateCommandList() override;
	void Submit(CommandList& commandList) override;
	void WaitIdle() override;
	bool ReadTexture(Texture& texture, void* outData, size_t outSize) override;

	[[nodiscard]] VkDevice GetDevice() const
	{
		return m_device;
	}

	[[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const
	{
		return m_physicalDevice;
	}

	[[nodiscard]] VkQueue GetQueue() const
	{
		return m_queue;
	}

	[[nodiscard]] uint32_t GetQueueFamily() const
	{
		return m_queueFamily;
	}

	bool ImmediateSubmit(const std::function<void(VkCommandBuffer)>& record);

private:
	bool CreateInstance();
	bool PickPhysicalDevice();
	bool CreateDevice();
	bool UploadTexture(VkTexture& texture, const void* data);
	void Destroy();

	VkInstance m_instance = VK_NULL_HANDLE;
	VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
	VkDevice m_device = VK_NULL_HANDLE;
	VkQueue m_queue = VK_NULL_HANDLE;
	uint32_t m_queueFamily = 0;
	VkCommandPool m_uploadPool = VK_NULL_HANDLE;
	VkCommandBuffer m_uploadCommandBuffer = VK_NULL_HANDLE;
	VkPhysicalDeviceProperties m_properties{};
	std::string m_deviceName;
	std::string m_apiVersion;
	int m_maxTextureSize = 0;
};

class VkCommandList final : public CommandList
{
public:
	explicit VkCommandList(VulkanDevice& device) : m_owner(device) { }

	~VkCommandList() override;

	bool Init();

	void Begin() override;
	void End() override;
	void BeginRenderPass(const RenderPassDesc& desc) override;
	void EndRenderPass() override;
	void SetViewport(const Viewport& viewport) override;
	void BindPipeline(Pipeline& pipeline) override;
	void BindVertexBuffer(Buffer& buffer) override;
	void BindIndexBuffer(Buffer& buffer) override;
	void BindUniformBuffer(uint32_t slot, Buffer& buffer, uint64_t offset, uint64_t size) override;
	void BindTexture(uint32_t slot, Texture& texture) override;
	void Draw(uint32_t vertexCount, uint32_t firstVertex) override;
	void DrawIndexed(uint32_t indexCount, uint32_t firstIndex) override;

	[[nodiscard]] VkCommandBuffer GetCommandBuffer() const
	{
		return m_commandBuffer;
	}

	[[nodiscard]] bool IsReady() const
	{
		return m_ready;
	}

private:
	VulkanDevice& m_owner;
	VkCommandPool m_pool = VK_NULL_HANDLE;
	VkCommandBuffer m_commandBuffer = VK_NULL_HANDLE;
	VkRhiPipeline* m_pipeline = nullptr;
	VkBuffer m_vertexBuffer = VK_NULL_HANDLE;
	VkBuffer m_indexBuffer = VK_NULL_HANDLE;
	Viewport m_viewport;
	bool m_recording = false;
	bool m_inPass = false;
	bool m_ready = false;
};

VkCommandList::~VkCommandList()
{
	if (m_pool != VK_NULL_HANDLE)
		vkDestroyCommandPool(m_owner.GetDevice(), m_pool, nullptr);
}

bool VkCommandList::Init()
{
	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = m_owner.GetQueueFamily();

	VkResult result = vkCreateCommandPool(m_owner.GetDevice(), &poolInfo, nullptr, &m_pool);
	if (result != VK_SUCCESS)
	{
		LogVulkan("vkCreateCommandPool failed", result);
		return false;
	}

	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = m_pool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	result = vkAllocateCommandBuffers(m_owner.GetDevice(), &allocateInfo, &m_commandBuffer);
	if (result != VK_SUCCESS)
	{
		LogVulkan("vkAllocateCommandBuffers failed", result);
		return false;
	}
	return true;
}

void VkCommandList::Begin()
{
	vkResetCommandPool(m_owner.GetDevice(), m_pool, 0);
	m_pipeline = nullptr;
	m_vertexBuffer = VK_NULL_HANDLE;
	m_indexBuffer = VK_NULL_HANDLE;
	m_inPass = false;
	m_ready = false;

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	const VkResult result = vkBeginCommandBuffer(m_commandBuffer, &beginInfo);
	m_recording = result == VK_SUCCESS;
	if (!m_recording)
		LogVulkan("vkBeginCommandBuffer failed", result);
}

void VkCommandList::End()
{
	if (!m_recording)
		return;
	const VkResult result = vkEndCommandBuffer(m_commandBuffer);
	if (result != VK_SUCCESS)
		LogVulkan("vkEndCommandBuffer failed", result);
	m_recording = false;
	m_ready = result == VK_SUCCESS;
}

void VkCommandList::BeginRenderPass(const RenderPassDesc& desc)
{
	if (desc.colorTarget == nullptr)
	{
		LogVulkan("The Vulkan backend needs an explicit color target", VK_ERROR_FEATURE_NOT_PRESENT);
		return;
	}
	if (!m_recording)
		return;

	VkTexture& color = static_cast<VkTexture&>(*desc.colorTarget);
	TransitionImage(m_commandBuffer, color.GetImage(), color.GetAspect(), color.layout,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	color.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkRenderingAttachmentInfo colorAttachment{};
	colorAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
	colorAttachment.imageView = color.GetView();
	colorAttachment.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	colorAttachment.loadOp = desc.colorLoadOp == LoadOp::Load ? VK_ATTACHMENT_LOAD_OP_LOAD
		: (desc.colorLoadOp == LoadOp::DontCare ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_CLEAR);
	colorAttachment.storeOp = desc.colorStoreOp == StoreOp::Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
	colorAttachment.clearValue.color.float32[0] = desc.clear.r;
	colorAttachment.clearValue.color.float32[1] = desc.clear.g;
	colorAttachment.clearValue.color.float32[2] = desc.clear.b;
	colorAttachment.clearValue.color.float32[3] = desc.clear.a;

	VkRenderingAttachmentInfo depthAttachment{};
	if (desc.depthTarget != nullptr)
	{
		VkTexture& depth = static_cast<VkTexture&>(*desc.depthTarget);
		TransitionImage(m_commandBuffer, depth.GetImage(), depth.GetAspect(), depth.layout,
			VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
			VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
		depth.layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

		depthAttachment.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
		depthAttachment.imageView = depth.GetView();
		depthAttachment.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
		depthAttachment.loadOp = desc.depthLoadOp == LoadOp::Load ? VK_ATTACHMENT_LOAD_OP_LOAD
			: (desc.depthLoadOp == LoadOp::DontCare ? VK_ATTACHMENT_LOAD_OP_DONT_CARE : VK_ATTACHMENT_LOAD_OP_CLEAR);
		depthAttachment.storeOp = desc.depthStoreOp == StoreOp::Store ? VK_ATTACHMENT_STORE_OP_STORE : VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depthAttachment.clearValue.depthStencil.depth = desc.clear.depth;
		depthAttachment.clearValue.depthStencil.stencil = desc.clear.stencil;
	}

	VkRenderingInfo renderingInfo{};
	renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
	renderingInfo.renderArea.offset.x = 0;
	renderingInfo.renderArea.offset.y = 0;
	renderingInfo.renderArea.extent.width = color.GetDesc().width;
	renderingInfo.renderArea.extent.height = color.GetDesc().height;
	renderingInfo.layerCount = 1;
	renderingInfo.colorAttachmentCount = 1;
	renderingInfo.pColorAttachments = &colorAttachment;
	renderingInfo.pDepthAttachment = desc.depthTarget != nullptr ? &depthAttachment : nullptr;

	vkCmdBeginRendering(m_commandBuffer, &renderingInfo);
	m_inPass = true;

	Viewport viewport;
	viewport.width = static_cast<float>(color.GetDesc().width);
	viewport.height = static_cast<float>(color.GetDesc().height);
	SetViewport(viewport);
}

void VkCommandList::EndRenderPass()
{
	if (!m_recording || !m_inPass)
		return;
	vkCmdEndRendering(m_commandBuffer);
	m_inPass = false;
}

void VkCommandList::SetViewport(const Viewport& viewport)
{
	m_viewport = viewport;
	if (!m_recording)
		return;

	VkViewport vkViewport{};
	vkViewport.x = viewport.x;
	vkViewport.y = viewport.y;
	vkViewport.width = viewport.width;
	vkViewport.height = viewport.height;
	vkViewport.minDepth = viewport.minDepth;
	vkViewport.maxDepth = viewport.maxDepth;
	vkCmdSetViewport(m_commandBuffer, 0, 1, &vkViewport);

	VkRect2D scissor{};
	scissor.offset.x = static_cast<int32_t>(viewport.x);
	scissor.offset.y = static_cast<int32_t>(viewport.y);
	scissor.extent.width = static_cast<uint32_t>(viewport.width);
	scissor.extent.height = static_cast<uint32_t>(viewport.height);
	vkCmdSetScissor(m_commandBuffer, 0, 1, &scissor);
}

void VkCommandList::BindPipeline(Pipeline& pipeline)
{
	m_pipeline = static_cast<VkRhiPipeline*>(&pipeline);
}

void VkCommandList::BindVertexBuffer(Buffer& buffer)
{
	VkRhiBuffer& vkBuffer = static_cast<VkRhiBuffer&>(buffer);
	if (vkBuffer.GetUsage() != BufferUsage::Vertex)
		return;
	m_vertexBuffer = vkBuffer.GetBuffer();
}

void VkCommandList::BindIndexBuffer(Buffer& buffer)
{
	VkRhiBuffer& vkBuffer = static_cast<VkRhiBuffer&>(buffer);
	if (vkBuffer.GetUsage() != BufferUsage::Index)
		return;
	m_indexBuffer = vkBuffer.GetBuffer();
}

void VkCommandList::BindUniformBuffer(uint32_t slot, Buffer& buffer, uint64_t offset, uint64_t size)
{
	// Descriptor bindings are the next Vulkan milestone.
	(void)slot;
	(void)buffer;
	(void)offset;
	(void)size;
}

void VkCommandList::BindTexture(uint32_t slot, Texture& texture)
{
	// Descriptor bindings are the next Vulkan milestone.
	(void)slot;
	(void)texture;
}

void VkCommandList::Draw(uint32_t vertexCount, uint32_t firstVertex)
{
	if (!m_recording || !m_inPass || m_pipeline == nullptr || m_vertexBuffer == VK_NULL_HANDLE)
		return;

	vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->GetPipeline());
	const VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &m_vertexBuffer, &offset);
	vkCmdDraw(m_commandBuffer, vertexCount, 1, firstVertex, 0);
}

void VkCommandList::DrawIndexed(uint32_t indexCount, uint32_t firstIndex)
{
	if (!m_recording || !m_inPass || m_pipeline == nullptr || m_vertexBuffer == VK_NULL_HANDLE || m_indexBuffer == VK_NULL_HANDLE)
		return;

	vkCmdBindPipeline(m_commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->GetPipeline());
	const VkDeviceSize offset = 0;
	vkCmdBindVertexBuffers(m_commandBuffer, 0, 1, &m_vertexBuffer, &offset);
	vkCmdBindIndexBuffer(m_commandBuffer, m_indexBuffer, 0, VK_INDEX_TYPE_UINT32);
	vkCmdDrawIndexed(m_commandBuffer, indexCount, 1, firstIndex, 0, 0);
}

VulkanDevice::~VulkanDevice()
{
	Destroy();
}

bool VulkanDevice::Init()
{
	if (!CreateInstance())
		return false;
	if (!PickPhysicalDevice())
	{
		Destroy();
		return false;
	}
	if (!CreateDevice())
	{
		Destroy();
		return false;
	}
	return true;
}

bool VulkanDevice::CreateInstance()
{
	VkApplicationInfo applicationInfo{};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pApplicationName = "Bones Engine";
	applicationInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
	applicationInfo.pEngineName = "Bones Engine";
	applicationInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
	applicationInfo.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo instanceInfo{};
	instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceInfo.pApplicationInfo = &applicationInfo;

	const VkResult result = vkCreateInstance(&instanceInfo, nullptr, &m_instance);
	if (result != VK_SUCCESS)
	{
		LogVulkan("vkCreateInstance failed", result);
		return false;
	}
	return true;
}

bool VulkanDevice::PickPhysicalDevice()
{
	uint32_t deviceCount = 0;
	VkResult result = vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
	if (result != VK_SUCCESS || deviceCount == 0)
	{
		LogVulkan("No Vulkan physical device", result);
		return false;
	}

	std::vector<VkPhysicalDevice> devices(deviceCount);
	vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

	for (VkPhysicalDevice device : devices)
	{
		VkPhysicalDeviceProperties properties{};
		vkGetPhysicalDeviceProperties(device, &properties);
		if (properties.apiVersion < VK_API_VERSION_1_3)
			continue;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		uint32_t graphicsFamily = UINT32_MAX;
		for (uint32_t i = 0; i < queueFamilyCount; ++i)
		{
			if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0)
			{
				graphicsFamily = i;
				break;
			}
		}
		if (graphicsFamily == UINT32_MAX)
			continue;

		VkPhysicalDeviceVulkan13Features supportedFeatures{};
		supportedFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
		VkPhysicalDeviceFeatures2 features{};
		features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
		features.pNext = &supportedFeatures;
		vkGetPhysicalDeviceFeatures2(device, &features);
		if (supportedFeatures.dynamicRendering != VK_TRUE)
			continue;

		m_physicalDevice = device;
		m_properties = properties;
		m_queueFamily = graphicsFamily;
		m_deviceName = properties.deviceName;
		m_apiVersion = "Vulkan " + std::to_string(VK_VERSION_MAJOR(properties.apiVersion)) + "."
			+ std::to_string(VK_VERSION_MINOR(properties.apiVersion)) + "."
			+ std::to_string(VK_VERSION_PATCH(properties.apiVersion));
		m_maxTextureSize = static_cast<int>(properties.limits.maxImageDimension2D);
		return true;
	}

	LogVulkan("No Vulkan 1.3 device with dynamic rendering", VK_ERROR_INCOMPATIBLE_DRIVER);
	return false;
}

bool VulkanDevice::CreateDevice()
{
	const float queuePriority = 1.0f;
	VkDeviceQueueCreateInfo queueInfo{};
	queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueInfo.queueFamilyIndex = m_queueFamily;
	queueInfo.queueCount = 1;
	queueInfo.pQueuePriorities = &queuePriority;

	VkPhysicalDeviceVulkan13Features features{};
	features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features.dynamicRendering = VK_TRUE;

	VkDeviceCreateInfo deviceInfo{};
	deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceInfo.pNext = &features;
	deviceInfo.queueCreateInfoCount = 1;
	deviceInfo.pQueueCreateInfos = &queueInfo;

	VkResult result = vkCreateDevice(m_physicalDevice, &deviceInfo, nullptr, &m_device);
	if (result != VK_SUCCESS)
	{
		LogVulkan("vkCreateDevice failed", result);
		return false;
	}
	vkGetDeviceQueue(m_device, m_queueFamily, 0, &m_queue);

	VkCommandPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	poolInfo.queueFamilyIndex = m_queueFamily;

	result = vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_uploadPool);
	if (result != VK_SUCCESS)
	{
		LogVulkan("vkCreateCommandPool failed", result);
		return false;
	}

	VkCommandBufferAllocateInfo allocateInfo{};
	allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocateInfo.commandPool = m_uploadPool;
	allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	allocateInfo.commandBufferCount = 1;

	result = vkAllocateCommandBuffers(m_device, &allocateInfo, &m_uploadCommandBuffer);
	if (result != VK_SUCCESS)
	{
		LogVulkan("vkAllocateCommandBuffers failed", result);
		return false;
	}
	return true;
}

void VulkanDevice::Destroy()
{
	if (m_device != VK_NULL_HANDLE)
	{
		vkDeviceWaitIdle(m_device);
		if (m_uploadPool != VK_NULL_HANDLE)
			vkDestroyCommandPool(m_device, m_uploadPool, nullptr);
		vkDestroyDevice(m_device, nullptr);
		m_device = VK_NULL_HANDLE;
	}
	if (m_instance != VK_NULL_HANDLE)
	{
		vkDestroyInstance(m_instance, nullptr);
		m_instance = VK_NULL_HANDLE;
	}
}

bool VulkanDevice::ImmediateSubmit(const std::function<void(VkCommandBuffer)>& record)
{
	if (m_uploadCommandBuffer == VK_NULL_HANDLE)
		return false;

	VkResult result = vkResetCommandBuffer(m_uploadCommandBuffer, 0);
	if (result != VK_SUCCESS)
		return false;

	VkCommandBufferBeginInfo beginInfo{};
	beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	result = vkBeginCommandBuffer(m_uploadCommandBuffer, &beginInfo);
	if (result != VK_SUCCESS)
		return false;

	record(m_uploadCommandBuffer);

	result = vkEndCommandBuffer(m_uploadCommandBuffer);
	if (result != VK_SUCCESS)
		return false;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence fence = VK_NULL_HANDLE;
	result = vkCreateFence(m_device, &fenceInfo, nullptr, &fence);
	if (result != VK_SUCCESS)
		return false;

	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &m_uploadCommandBuffer;

	result = vkQueueSubmit(m_queue, 1, &submitInfo, fence);
	if (result == VK_SUCCESS)
		result = vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);

	vkDestroyFence(m_device, fence, nullptr);
	return result == VK_SUCCESS;
}

bool VulkanDevice::UploadTexture(VkTexture& texture, const void* data)
{
	const TextureDesc& desc = texture.GetDesc();
	if (desc.format != Format::R8G8B8A8_UNorm || data == nullptr)
		return false;

	const VkDeviceSize size = static_cast<VkDeviceSize>(desc.width) * desc.height * 4;

	VkBuffer stagingBuffer = VK_NULL_HANDLE;
	VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
	void* mapped = nullptr;
	if (!CreateHostBuffer(m_physicalDevice, m_device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, stagingBuffer, stagingMemory, mapped))
		return false;
	if (mapped == nullptr)
	{
		vkDestroyBuffer(m_device, stagingBuffer, nullptr);
		vkFreeMemory(m_device, stagingMemory, nullptr);
		return false;
	}
	std::memcpy(mapped, data, static_cast<size_t>(size));
	vkUnmapMemory(m_device, stagingMemory);

	const bool uploaded = ImmediateSubmit([&](VkCommandBuffer commandBuffer)
	{
		TransitionImage(commandBuffer, texture.GetImage(), texture.GetAspect(),
			texture.layout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);

		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0;
		region.bufferImageHeight = 0;
		region.imageSubresource.aspectMask = texture.GetAspect();
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { desc.width, desc.height, 1 };
		vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texture.GetImage(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

		TransitionImage(commandBuffer, texture.GetImage(), texture.GetAspect(),
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
			VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
	});
	texture.layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

	vkDestroyBuffer(m_device, stagingBuffer, nullptr);
	vkFreeMemory(m_device, stagingMemory, nullptr);
	return uploaded;
}

std::unique_ptr<Buffer> VulkanDevice::CreateBuffer(const BufferDesc& desc, const void* initialData)
{
	auto buffer = std::make_unique<VkRhiBuffer>(desc);
	if (!buffer->Create(m_physicalDevice, m_device, initialData))
		return nullptr;
	return buffer;
}

void VulkanDevice::UpdateBuffer(Buffer& buffer, const void* data, uint64_t size, uint64_t offset)
{
	static_cast<VkRhiBuffer&>(buffer).Update(data, size, offset);
}

std::unique_ptr<Texture> VulkanDevice::CreateTexture(const TextureDesc& desc, const void* initialData)
{
	auto texture = std::make_unique<VkTexture>(m_device, m_physicalDevice, desc);
	if (!texture->Create())
		return nullptr;
	if (initialData != nullptr && !UploadTexture(*texture, initialData))
		return nullptr;
	return texture;
}

std::unique_ptr<Shader> VulkanDevice::CreateShader(const ShaderDesc& desc)
{
	auto shader = std::make_unique<VkShader>(desc);
	if (!shader->Create(m_device))
		return nullptr;
	return shader;
}

std::unique_ptr<Pipeline> VulkanDevice::CreatePipeline(const PipelineDesc& desc)
{
	auto pipeline = std::make_unique<VkRhiPipeline>(desc);
	if (!pipeline->Create(m_device))
		return nullptr;
	return pipeline;
}

std::unique_ptr<CommandList> VulkanDevice::CreateCommandList()
{
	auto commandList = std::make_unique<VkCommandList>(*this);
	if (!commandList->Init())
		return nullptr;
	return commandList;
}

void VulkanDevice::Submit(CommandList& commandList)
{
	VkCommandList& vulkanCommandList = static_cast<VkCommandList&>(commandList);
	if (!vulkanCommandList.IsReady())
		return;

	VkFenceCreateInfo fenceInfo{};
	fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	VkFence fence = VK_NULL_HANDLE;
	VkResult result = vkCreateFence(m_device, &fenceInfo, nullptr, &fence);
	if (result != VK_SUCCESS)
	{
		LogVulkan("vkCreateFence failed", result);
		return;
	}

	VkCommandBuffer commandBuffer = vulkanCommandList.GetCommandBuffer();
	VkSubmitInfo submitInfo{};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	result = vkQueueSubmit(m_queue, 1, &submitInfo, fence);
	if (result == VK_SUCCESS)
		result = vkWaitForFences(m_device, 1, &fence, VK_TRUE, UINT64_MAX);
	if (result != VK_SUCCESS)
		LogVulkan("vkQueueSubmit failed", result);

	vkDestroyFence(m_device, fence, nullptr);
}

void VulkanDevice::WaitIdle()
{
	vkDeviceWaitIdle(m_device);
}

bool VulkanDevice::ReadTexture(Texture& texture, void* outData, size_t outSize)
{
	VkTexture& vkTexture = static_cast<VkTexture&>(texture);
	const TextureDesc& desc = vkTexture.GetDesc();
	if (desc.format != Format::R8G8B8A8_UNorm || outData == nullptr)
		return false;
	const size_t bytes = static_cast<size_t>(desc.width) * desc.height * 4;
	if (outSize < bytes)
		return false;

	VkBuffer readbackBuffer = VK_NULL_HANDLE;
	VkDeviceMemory readbackMemory = VK_NULL_HANDLE;
	void* mapped = nullptr;
	if (!CreateHostBuffer(m_physicalDevice, m_device, bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, readbackBuffer, readbackMemory, mapped))
		return false;
	if (mapped == nullptr)
	{
		vkDestroyBuffer(m_device, readbackBuffer, nullptr);
		vkFreeMemory(m_device, readbackMemory, nullptr);
		return false;
	}

	const bool copied = ImmediateSubmit([&](VkCommandBuffer commandBuffer)
	{
		TransitionImage(commandBuffer, vkTexture.GetImage(), vkTexture.GetAspect(),
			vkTexture.layout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);

		VkBufferImageCopy region{};
		region.imageSubresource.aspectMask = vkTexture.GetAspect();
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;
		region.imageOffset = { 0, 0, 0 };
		region.imageExtent = { desc.width, desc.height, 1 };
		vkCmdCopyImageToBuffer(commandBuffer, vkTexture.GetImage(),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, readbackBuffer, 1, &region);

		TransitionImage(commandBuffer, vkTexture.GetImage(), vkTexture.GetAspect(),
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
	});
	vkTexture.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	if (copied && mapped != nullptr)
		std::memcpy(outData, mapped, bytes);

	if (mapped != nullptr)
		vkUnmapMemory(m_device, readbackMemory);
	vkDestroyBuffer(m_device, readbackBuffer, nullptr);
	vkFreeMemory(m_device, readbackMemory, nullptr);
	return copied;
}

} // namespace

std::unique_ptr<Device> CreateVulkanDevice()
{
	auto device = std::make_unique<VulkanDevice>();
	if (!device->Init())
		return nullptr;
	return device;
}

} // namespace bones::rhi

#endif

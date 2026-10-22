// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include "../unit_test_manager.h"

#if defined(BONES_VULKAN)

#include <engine/debug/debug.h>
#include <engine/graphics/rhi/rhi_vulkan.h>

#include <cstdlib>
#include <fstream>
#include <vector>

namespace
{
	std::vector<uint32_t> LoadSpirv(const char* path, bool& loaded)
	{
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file)
		{
			loaded = false;
			return {};
		}

		const std::streamsize size = file.tellg();
		if (size <= 0 || size % 4 != 0)
		{
			loaded = false;
			return {};
		}

		std::vector<uint32_t> words(static_cast<size_t>(size) / sizeof(uint32_t));
		file.seekg(0, std::ios::beg);
		file.read(reinterpret_cast<char*>(words.data()), size);
		loaded = file.good();
		return words;
	}
}

TestResult RendererContractVulkanTest::Start(std::string& errorOut)
{
	BEGIN_TEST();

	std::unique_ptr<bones::rhi::Device> device = bones::rhi::CreateVulkanDevice();
	if (!device)
	{
		if (std::getenv("BONES_REQUIRE_VULKAN") != nullptr)
		{
			EXPECT_TRUE(false, "Vulkan device creation failed");
		}
		else
		{
			Debug::PrintWarning("No Vulkan device available, skipping the renderer contract test", true);
		}
		return testResult;
	}

	bool vertexLoaded = false;
	bool fragmentLoaded = false;
	const std::vector<uint32_t> vertexCode = LoadSpirv("test_shaders/triangle.vert.spv", vertexLoaded);
	const std::vector<uint32_t> fragmentCode = LoadSpirv("test_shaders/triangle.frag.spv", fragmentLoaded);
	EXPECT_TRUE(vertexLoaded, "Failed to load the Vulkan vertex shader");
	EXPECT_TRUE(fragmentLoaded, "Failed to load the Vulkan fragment shader");
	if (!vertexLoaded || !fragmentLoaded)
		return testResult;

	bones::rhi::RenderTargetDesc targetDesc;
	targetDesc.width = 64;
	targetDesc.height = 64;
	targetDesc.colorFormat = bones::rhi::Format::R8G8B8A8_UNorm;
	targetDesc.depthFormat = bones::rhi::Format::Undefined;
	std::unique_ptr<bones::rhi::RenderTarget> target = device->CreateRenderTarget(targetDesc);
	EXPECT_NOT_NULL(target, "Failed to create the Vulkan render target");
	if (!target)
		return testResult;

	bones::rhi::ShaderDesc vertexDesc;
	vertexDesc.stage = bones::rhi::ShaderStage::Vertex;
	vertexDesc.spirv = vertexCode.data();
	vertexDesc.spirvSize = vertexCode.size() * sizeof(uint32_t);
	std::unique_ptr<bones::rhi::Shader> vertexShader = device->CreateShader(vertexDesc);
	EXPECT_NOT_NULL(vertexShader, "Failed to create the Vulkan vertex shader");
	if (!vertexShader)
		return testResult;

	bones::rhi::ShaderDesc fragmentDesc;
	fragmentDesc.stage = bones::rhi::ShaderStage::Fragment;
	fragmentDesc.spirv = fragmentCode.data();
	fragmentDesc.spirvSize = fragmentCode.size() * sizeof(uint32_t);
	std::unique_ptr<bones::rhi::Shader> fragmentShader = device->CreateShader(fragmentDesc);
	EXPECT_NOT_NULL(fragmentShader, "Failed to create the Vulkan fragment shader");
	if (!fragmentShader)
		return testResult;

	bones::rhi::PipelineDesc pipelineDesc;
	pipelineDesc.vertexShader = vertexShader.get();
	pipelineDesc.fragmentShader = fragmentShader.get();
	pipelineDesc.vertexLayout.stride = 6 * sizeof(float);
	pipelineDesc.vertexLayout.attributes = {
		{ 0, bones::rhi::Format::R32G32_Float, 0 },
		{ 1, bones::rhi::Format::R32G32B32A32_Float, 2 * sizeof(float) },
	};
	pipelineDesc.cullMode = bones::rhi::CullMode::None;
	std::unique_ptr<bones::rhi::Pipeline> pipeline = device->CreatePipeline(pipelineDesc);
	EXPECT_NOT_NULL(pipeline, "Failed to create the Vulkan pipeline");
	if (!pipeline)
		return testResult;

	const float vertices[] = {
		-0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f,
		 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f,
		 0.0f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f,
	};
	bones::rhi::BufferDesc vertexBufferDesc;
	vertexBufferDesc.size = sizeof(vertices);
	vertexBufferDesc.usage = bones::rhi::BufferUsage::Vertex;
	std::unique_ptr<bones::rhi::Buffer> vertexBuffer = device->CreateBuffer(vertexBufferDesc, vertices);
	EXPECT_NOT_NULL(vertexBuffer, "Failed to create the Vulkan vertex buffer");
	if (!vertexBuffer)
		return testResult;

	std::unique_ptr<bones::rhi::CommandList> commandList = device->CreateCommandList();
	EXPECT_NOT_NULL(commandList, "Failed to create the Vulkan command list");
	if (!commandList)
		return testResult;

	bones::rhi::RenderPassDesc passDesc;
	passDesc.colorTarget = &target->GetColorTexture();
	passDesc.colorLoadOp = bones::rhi::LoadOp::Clear;
	passDesc.clear.r = 0.0f;
	passDesc.clear.g = 0.0f;
	passDesc.clear.b = 1.0f;
	passDesc.clear.a = 1.0f;

	commandList->Begin();
	commandList->BeginRenderPass(passDesc);
	commandList->BindPipeline(*pipeline);
	commandList->BindVertexBuffer(*vertexBuffer);
	commandList->Draw(3);
	commandList->EndRenderPass();
	commandList->End();
	device->Submit(*commandList);
	device->WaitIdle();

	unsigned char pixels[64 * 64 * 4] = {};
	EXPECT_TRUE(device->ReadTexture(target->GetColorTexture(), pixels, sizeof(pixels)), "Failed to read the Vulkan render target");

	const size_t center = (32 * 64 + 32) * 4;
	EXPECT_TRUE(pixels[center + 0] > 240 && pixels[center + 1] < 15 && pixels[center + 2] < 15, "The Vulkan triangle was not drawn at the centre");

	const size_t corner = (2 * 64 + 2) * 4;
	EXPECT_TRUE(pixels[corner + 0] < 15 && pixels[corner + 1] < 15 && pixels[corner + 2] > 240, "The Vulkan render target was not cleared");

	return testResult;
}

TestResult RendererContractResourcesVulkanTest::Start(std::string& errorOut)
{
	BEGIN_TEST();

	std::unique_ptr<bones::rhi::Device> device = bones::rhi::CreateVulkanDevice();
	if (!device)
	{
		if (std::getenv("BONES_REQUIRE_VULKAN") != nullptr)
		{
			EXPECT_TRUE(false, "Vulkan device creation failed");
		}
		else
		{
			Debug::PrintWarning("No Vulkan device available, skipping the renderer resources test", true);
		}
		return testResult;
	}

	bool vertexLoaded = false;
	bool fragmentLoaded = false;
	const std::vector<uint32_t> vertexCode = LoadSpirv("test_shaders/quad.vert.spv", vertexLoaded);
	const std::vector<uint32_t> fragmentCode = LoadSpirv("test_shaders/quad.frag.spv", fragmentLoaded);
	EXPECT_TRUE(vertexLoaded, "Failed to load the Vulkan quad vertex shader");
	EXPECT_TRUE(fragmentLoaded, "Failed to load the Vulkan quad fragment shader");
	if (!vertexLoaded || !fragmentLoaded)
		return testResult;

	bones::rhi::RenderTargetDesc targetDesc;
	targetDesc.width = 64;
	targetDesc.height = 64;
	targetDesc.colorFormat = bones::rhi::Format::R8G8B8A8_UNorm;
	targetDesc.depthFormat = bones::rhi::Format::Undefined;
	std::unique_ptr<bones::rhi::RenderTarget> target = device->CreateRenderTarget(targetDesc);
	EXPECT_NOT_NULL(target, "Failed to create the Vulkan resources render target");
	if (!target)
		return testResult;

	unsigned char whiteTexture[2 * 2 * 4];
	for (unsigned char& value : whiteTexture)
		value = 255;

	bones::rhi::TextureDesc textureDesc;
	textureDesc.width = 2;
	textureDesc.height = 2;
	textureDesc.format = bones::rhi::Format::R8G8B8A8_UNorm;
	textureDesc.usage = bones::rhi::TextureUsage::Sampled;
	textureDesc.minFilter = bones::rhi::Filter::Nearest;
	textureDesc.magFilter = bones::rhi::Filter::Nearest;
	textureDesc.wrap = bones::rhi::WrapMode::ClampToEdge;
	std::unique_ptr<bones::rhi::Texture> texture = device->CreateTexture(textureDesc, whiteTexture);
	EXPECT_NOT_NULL(texture, "Failed to create the Vulkan test texture");
	if (!texture)
		return testResult;

	const float tint[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
	bones::rhi::BufferDesc uniformDesc;
	uniformDesc.size = sizeof(tint);
	uniformDesc.usage = bones::rhi::BufferUsage::Uniform;
	std::unique_ptr<bones::rhi::Buffer> uniformBuffer = device->CreateBuffer(uniformDesc, tint);
	EXPECT_NOT_NULL(uniformBuffer, "Failed to create the Vulkan uniform buffer");
	if (!uniformBuffer)
		return testResult;

	bones::rhi::ShaderDesc vertexDesc;
	vertexDesc.stage = bones::rhi::ShaderStage::Vertex;
	vertexDesc.spirv = vertexCode.data();
	vertexDesc.spirvSize = vertexCode.size() * sizeof(uint32_t);
	std::unique_ptr<bones::rhi::Shader> vertexShader = device->CreateShader(vertexDesc);
	EXPECT_NOT_NULL(vertexShader, "Failed to create the Vulkan quad vertex shader");
	if (!vertexShader)
		return testResult;

	bones::rhi::ShaderDesc fragmentDesc;
	fragmentDesc.stage = bones::rhi::ShaderStage::Fragment;
	fragmentDesc.spirv = fragmentCode.data();
	fragmentDesc.spirvSize = fragmentCode.size() * sizeof(uint32_t);
	std::unique_ptr<bones::rhi::Shader> fragmentShader = device->CreateShader(fragmentDesc);
	EXPECT_NOT_NULL(fragmentShader, "Failed to create the Vulkan quad fragment shader");
	if (!fragmentShader)
		return testResult;

	bones::rhi::PipelineDesc pipelineDesc;
	pipelineDesc.vertexShader = vertexShader.get();
	pipelineDesc.fragmentShader = fragmentShader.get();
	pipelineDesc.vertexLayout.stride = 4 * sizeof(float);
	pipelineDesc.vertexLayout.attributes = {
		{ 0, bones::rhi::Format::R32G32_Float, 0 },
		{ 1, bones::rhi::Format::R32G32_Float, 2 * sizeof(float) },
	};
	pipelineDesc.cullMode = bones::rhi::CullMode::None;
	pipelineDesc.descriptorBindings = {
		{ 0, bones::rhi::DescriptorType::UniformBuffer, bones::rhi::ShaderStageFlags::Vertex | bones::rhi::ShaderStageFlags::Fragment },
		{ 1, bones::rhi::DescriptorType::Texture, bones::rhi::ShaderStageFlags::Fragment },
	};
	std::unique_ptr<bones::rhi::Pipeline> pipeline = device->CreatePipeline(pipelineDesc);
	EXPECT_NOT_NULL(pipeline, "Failed to create the Vulkan resources pipeline");
	if (!pipeline)
		return testResult;

	const float vertices[] = {
		-0.8f, -0.8f, 0.0f, 0.0f,
		 0.8f, -0.8f, 1.0f, 0.0f,
		 0.8f,  0.8f, 1.0f, 1.0f,
		-0.8f,  0.8f, 0.0f, 1.0f,
	};
	bones::rhi::BufferDesc vertexBufferDesc;
	vertexBufferDesc.size = sizeof(vertices);
	vertexBufferDesc.usage = bones::rhi::BufferUsage::Vertex;
	std::unique_ptr<bones::rhi::Buffer> vertexBuffer = device->CreateBuffer(vertexBufferDesc, vertices);
	EXPECT_NOT_NULL(vertexBuffer, "Failed to create the Vulkan quad vertex buffer");
	if (!vertexBuffer)
		return testResult;

	const uint32_t indices[] = { 0, 1, 2, 0, 2, 3 };
	bones::rhi::BufferDesc indexBufferDesc;
	indexBufferDesc.size = sizeof(indices);
	indexBufferDesc.usage = bones::rhi::BufferUsage::Index;
	std::unique_ptr<bones::rhi::Buffer> indexBuffer = device->CreateBuffer(indexBufferDesc, indices);
	EXPECT_NOT_NULL(indexBuffer, "Failed to create the Vulkan quad index buffer");
	if (!indexBuffer)
		return testResult;

	std::unique_ptr<bones::rhi::CommandList> commandList = device->CreateCommandList();
	EXPECT_NOT_NULL(commandList, "Failed to create the Vulkan resources command list");
	if (!commandList)
		return testResult;

	bones::rhi::RenderPassDesc passDesc;
	passDesc.colorTarget = &target->GetColorTexture();
	passDesc.colorLoadOp = bones::rhi::LoadOp::Clear;
	passDesc.clear.r = 0.0f;
	passDesc.clear.g = 0.0f;
	passDesc.clear.b = 1.0f;
	passDesc.clear.a = 1.0f;

	commandList->Begin();
	commandList->BeginRenderPass(passDesc);
	commandList->BindPipeline(*pipeline);
	commandList->BindVertexBuffer(*vertexBuffer);
	commandList->BindIndexBuffer(*indexBuffer);
	commandList->BindUniformBuffer(0, *uniformBuffer);
	commandList->BindTexture(1, *texture);
	commandList->DrawIndexed(6);
	commandList->EndRenderPass();
	commandList->End();
	device->Submit(*commandList);
	device->WaitIdle();

	unsigned char pixels[64 * 64 * 4] = {};
	EXPECT_TRUE(device->ReadTexture(target->GetColorTexture(), pixels, sizeof(pixels)), "Failed to read the Vulkan resources render target");

	const size_t center = (32 * 64 + 32) * 4;
	EXPECT_TRUE(pixels[center + 0] < 15 && pixels[center + 1] > 240 && pixels[center + 2] < 15, "The Vulkan textured quad was not drawn at the centre");

	const size_t corner = (2 * 64 + 2) * 4;
	EXPECT_TRUE(pixels[corner + 0] < 15 && pixels[corner + 1] < 15 && pixels[corner + 2] > 240, "The Vulkan resources render target was not cleared");

	return testResult;
}

#endif

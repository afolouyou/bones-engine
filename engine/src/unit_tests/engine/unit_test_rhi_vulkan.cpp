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

	bones::rhi::TextureDesc targetDesc;
	targetDesc.width = 64;
	targetDesc.height = 64;
	targetDesc.format = bones::rhi::Format::R8G8B8A8_UNorm;
	targetDesc.usage = bones::rhi::TextureUsage::Sampled | bones::rhi::TextureUsage::RenderTarget;
	std::unique_ptr<bones::rhi::Texture> target = device->CreateTexture(targetDesc, nullptr);
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
	passDesc.colorTarget = target.get();
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
	EXPECT_TRUE(device->ReadTexture(*target, pixels, sizeof(pixels)), "Failed to read the Vulkan render target");

	const size_t center = (32 * 64 + 32) * 4;
	EXPECT_TRUE(pixels[center + 0] > 240 && pixels[center + 1] < 15 && pixels[center + 2] < 15, "The Vulkan triangle was not drawn at the centre");

	const size_t corner = (2 * 64 + 2) * 4;
	EXPECT_TRUE(pixels[corner + 0] < 15 && pixels[corner + 1] < 15 && pixels[corner + 2] > 240, "The Vulkan render target was not cleared");

	return testResult;
}

#endif

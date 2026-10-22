// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include "../unit_test_manager.h"

#if !defined(__PS3__)

#include <engine/graphics/rhi/rhi_opengl.h>
#include <engine/ui/window.h>

#include <glad/gl.h>

namespace
{
	const char* kVertexShader = R"(#version 330 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec4 a_Color;

out vec4 v_Color;

void main()
{
	gl_Position = vec4(a_Position, 0.0, 1.0);
	v_Color = a_Color;
}
)";

	const char* kFragmentShader = R"(#version 330 core
in vec4 v_Color;
out vec4 o_Color;

void main()
{
	o_Color = v_Color;
}
)";

	void RestoreRendererState()
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glUseProgram(0);
		glBindVertexArray(0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, 0);

		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_TRUE);
		glDepthFunc(GL_LESS);
		glDepthRange(0, 1);
		glEnable(GL_CULL_FACE);
		glCullFace(GL_BACK);
		glFrontFace(GL_CCW);
		glDisable(GL_BLEND);
		glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
		glViewport(0, 0, Window::GetWidth(), Window::GetHeight());
	}
}

TestResult RendererContractTest::Start(std::string& errorOut)
{
	BEGIN_TEST();

	std::unique_ptr<bones::rhi::Device> device = bones::rhi::CreateOpenGLDevice();
	EXPECT_NOT_NULL(device, "Failed to create the OpenGL device");
	if (!device)
		return testResult;

	// Render target
	bones::rhi::TextureDesc targetDesc;
	targetDesc.width = 64;
	targetDesc.height = 64;
	targetDesc.format = bones::rhi::Format::R8G8B8A8_UNorm;
	targetDesc.usage = bones::rhi::TextureUsage::Sampled | bones::rhi::TextureUsage::RenderTarget;
	std::unique_ptr<bones::rhi::Texture> target = device->CreateTexture(targetDesc, nullptr);
	EXPECT_NOT_NULL(target, "Failed to create the render target");
	if (!target)
		return testResult;

	// Shaders
	bones::rhi::ShaderDesc vertexDesc;
	vertexDesc.stage = bones::rhi::ShaderStage::Vertex;
	vertexDesc.glslSource = kVertexShader;
	std::unique_ptr<bones::rhi::Shader> vertexShader = device->CreateShader(vertexDesc);
	EXPECT_NOT_NULL(vertexShader, "Failed to compile the vertex shader");
	if (!vertexShader)
		return testResult;

	bones::rhi::ShaderDesc fragmentDesc;
	fragmentDesc.stage = bones::rhi::ShaderStage::Fragment;
	fragmentDesc.glslSource = kFragmentShader;
	std::unique_ptr<bones::rhi::Shader> fragmentShader = device->CreateShader(fragmentDesc);
	EXPECT_NOT_NULL(fragmentShader, "Failed to compile the fragment shader");
	if (!fragmentShader)
		return testResult;

	// Pipeline
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
	EXPECT_NOT_NULL(pipeline, "Failed to link the pipeline");
	if (!pipeline)
		return testResult;

	// A red triangle around the centre of the target.
	const float vertices[] = {
		-0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f,
		 0.5f, -0.5f, 1.0f, 0.0f, 0.0f, 1.0f,
		 0.0f,  0.5f, 1.0f, 0.0f, 0.0f, 1.0f,
	};
	bones::rhi::BufferDesc vertexBufferDesc;
	vertexBufferDesc.size = sizeof(vertices);
	vertexBufferDesc.usage = bones::rhi::BufferUsage::Vertex;
	std::unique_ptr<bones::rhi::Buffer> vertexBuffer = device->CreateBuffer(vertexBufferDesc, vertices);
	EXPECT_NOT_NULL(vertexBuffer, "Failed to create the vertex buffer");
	if (!vertexBuffer)
		return testResult;

	std::unique_ptr<bones::rhi::CommandList> commandList = device->CreateCommandList();
	EXPECT_NOT_NULL(commandList, "Failed to create the command list");
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
	EXPECT_TRUE(device->ReadTexture(*target, pixels, sizeof(pixels)), "Failed to read the render target");

	const size_t center = (32 * 64 + 32) * 4;
	EXPECT_TRUE(pixels[center + 0] > 240 && pixels[center + 1] < 15 && pixels[center + 2] < 15, "The triangle was not drawn at the centre");

	const size_t corner = (2 * 64 + 2) * 4;
	EXPECT_TRUE(pixels[corner + 0] < 15 && pixels[corner + 1] < 15 && pixels[corner + 2] > 240, "The render target was not cleared");

	RestoreRendererState();
	return testResult;
}

#endif

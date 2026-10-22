// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#pragma once

/**
 * @file rhi.h
 * @brief Backend-neutral rendering contract.
 *
 * The engine's high-level renderer talks to a Device and records work in a
 * CommandList. Nothing in this header depends on OpenGL, Vulkan or RSX, so a
 * new backend can be added without touching game code.
 *
 * Rules:
 * - Index buffers use 32-bit indices.
 * - Texture and buffer objects are owned by the caller (unique_ptr).
 * - Command lists are recorded on one thread and submitted on that thread.
 * - Bindings are explicit; there is no implicit global state.
 */

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace bones::rhi
{

enum class Backend : uint8_t
{
	OpenGL,
	Vulkan,
	RSX,
};

enum class Format : uint8_t
{
	Undefined,
	R8_UNorm,
	R8G8B8A8_UNorm,
	R8G8B8A8_SRGB,
	R32_Float,
	R32G32_Float,
	R32G32B32_Float,
	R32G32B32A32_Float,
	D24_UNorm_S8_UInt,
	D32_Float,
};

enum class BufferUsage : uint8_t
{
	Vertex,
	Index,
	Uniform,
};

enum class TextureUsage : uint8_t
{
	Sampled = 1 << 0,
	RenderTarget = 1 << 1,
	DepthStencil = 1 << 2,
};

inline TextureUsage operator|(TextureUsage left, TextureUsage right)
{
	return static_cast<TextureUsage>(static_cast<uint8_t>(left) | static_cast<uint8_t>(right));
}

inline bool HasTextureUsage(TextureUsage value, TextureUsage flag)
{
	return (static_cast<uint8_t>(value) & static_cast<uint8_t>(flag)) != 0;
}

enum class Filter : uint8_t
{
	Nearest,
	Linear,
};

enum class WrapMode : uint8_t
{
	Repeat,
	ClampToEdge,
};

enum class ShaderStage : uint8_t
{
	Vertex,
	Fragment,
	Compute,
};

enum class Topology : uint8_t
{
	TriangleList,
	LineList,
	PointList,
};

enum class CullMode : uint8_t
{
	None,
	Front,
	Back,
};

enum class FrontFace : uint8_t
{
	CounterClockwise,
	Clockwise,
};

enum class CompareOp : uint8_t
{
	Never,
	Less,
	Equal,
	LessEqual,
	Greater,
	GreaterEqual,
	Always,
};

enum class BlendMode : uint8_t
{
	Opaque,
	AlphaBlend,
};

enum class LoadOp : uint8_t
{
	Load,
	Clear,
	DontCare,
};

enum class StoreOp : uint8_t
{
	Store,
	DontCare,
};

class Buffer;
class Texture;
class Shader;
class Pipeline;
class CommandList;

struct BufferDesc
{
	uint64_t size = 0;
	BufferUsage usage = BufferUsage::Vertex;
	const char* debugName = nullptr;
};

struct TextureDesc
{
	uint32_t width = 0;
	uint32_t height = 0;
	Format format = Format::R8G8B8A8_UNorm;
	TextureUsage usage = TextureUsage::Sampled;
	Filter minFilter = Filter::Linear;
	Filter magFilter = Filter::Linear;
	WrapMode wrap = WrapMode::ClampToEdge;
	const char* debugName = nullptr;
};

struct ShaderDesc
{
	ShaderStage stage = ShaderStage::Vertex;
	/** GLSL source, used by the OpenGL backend. */
	const char* glslSource = nullptr;
	/** SPIR-V words, used by the Vulkan backend. */
	const void* spirv = nullptr;
	size_t spirvSize = 0;
	const char* entryPoint = "main";
};

struct VertexAttribute
{
	uint32_t location = 0;
	Format format = Format::R32G32B32_Float;
	uint32_t offset = 0;
};

struct VertexLayout
{
	uint32_t stride = 0;
	std::vector<VertexAttribute> attributes;
};

struct PipelineDesc
{
	Shader* vertexShader = nullptr;
	Shader* fragmentShader = nullptr;
	VertexLayout vertexLayout;
	Topology topology = Topology::TriangleList;
	CullMode cullMode = CullMode::None;
	FrontFace frontFace = FrontFace::CounterClockwise;
	bool depthTest = false;
	bool depthWrite = false;
	CompareOp depthCompare = CompareOp::Less;
	BlendMode blendMode = BlendMode::Opaque;
	Format colorFormat = Format::R8G8B8A8_UNorm;
	Format depthFormat = Format::Undefined;
};

struct Viewport
{
	float x = 0;
	float y = 0;
	float width = 0;
	float height = 0;
	float minDepth = 0;
	float maxDepth = 1;
};

struct ClearValue
{
	float r = 0;
	float g = 0;
	float b = 0;
	float a = 1;
	float depth = 1;
	uint32_t stencil = 0;
};

struct RenderPassDesc
{
	/** Null renders to the default framebuffer. */
	Texture* colorTarget = nullptr;
	Texture* depthTarget = nullptr;
	LoadOp colorLoadOp = LoadOp::Clear;
	StoreOp colorStoreOp = StoreOp::Store;
	LoadOp depthLoadOp = LoadOp::DontCare;
	StoreOp depthStoreOp = StoreOp::DontCare;
	ClearValue clear;
};

class Buffer
{
public:
	virtual ~Buffer() = default;
	[[nodiscard]] virtual uint64_t GetSize() const = 0;
};

class Texture
{
public:
	virtual ~Texture() = default;
	[[nodiscard]] virtual const TextureDesc& GetDesc() const = 0;
};

class Shader
{
public:
	virtual ~Shader() = default;
	[[nodiscard]] virtual ShaderStage GetStage() const = 0;
};

class Pipeline
{
public:
	virtual ~Pipeline() = default;
};

class CommandList
{
public:
	virtual ~CommandList() = default;

	virtual void Begin() = 0;
	virtual void End() = 0;

	virtual void BeginRenderPass(const RenderPassDesc& desc) = 0;
	virtual void EndRenderPass() = 0;
	virtual void SetViewport(const Viewport& viewport) = 0;

	virtual void BindPipeline(Pipeline& pipeline) = 0;
	virtual void BindVertexBuffer(Buffer& buffer) = 0;
	virtual void BindIndexBuffer(Buffer& buffer) = 0;
	virtual void BindUniformBuffer(uint32_t slot, Buffer& buffer, uint64_t offset = 0, uint64_t size = 0) = 0;
	virtual void BindTexture(uint32_t slot, Texture& texture) = 0;

	virtual void Draw(uint32_t vertexCount, uint32_t firstVertex = 0) = 0;
	virtual void DrawIndexed(uint32_t indexCount, uint32_t firstIndex = 0) = 0;
};

class Device
{
public:
	virtual ~Device() = default;

	[[nodiscard]] virtual Backend GetBackend() const = 0;
	[[nodiscard]] virtual const char* GetDeviceName() const = 0;
	[[nodiscard]] virtual const char* GetApiVersion() const = 0;
	[[nodiscard]] virtual int GetMaxTextureSize() const = 0;

	virtual std::unique_ptr<Buffer> CreateBuffer(const BufferDesc& desc, const void* initialData = nullptr) = 0;
	virtual void UpdateBuffer(Buffer& buffer, const void* data, uint64_t size, uint64_t offset = 0) = 0;

	virtual std::unique_ptr<Texture> CreateTexture(const TextureDesc& desc, const void* initialData = nullptr) = 0;

	virtual std::unique_ptr<Shader> CreateShader(const ShaderDesc& desc) = 0;
	virtual std::unique_ptr<Pipeline> CreatePipeline(const PipelineDesc& desc) = 0;

	virtual std::unique_ptr<CommandList> CreateCommandList() = 0;
	virtual void Submit(CommandList& commandList) = 0;
	virtual void WaitIdle() = 0;

	/**
	 * @brief Copy an RGBA8 render target back to CPU memory.
	 * @param outData Destination buffer, width * height * 4 bytes.
	 * @return false if the format is unsupported or the target is not readable.
	 */
	virtual bool ReadTexture(Texture& texture, void* outData, size_t outSize) = 0;
};

} // namespace bones::rhi

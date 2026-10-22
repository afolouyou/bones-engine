// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include "rhi_opengl.h"

#if !defined(__PS3__)

#include <glad/gl.h>

#include <cstdint>
#include <string>
#include <utility>

namespace bones::rhi
{
namespace
{

struct TextureFormatInfo
{
	GLint internalFormat = 0;
	GLenum format = 0;
	GLenum type = 0;
};

bool GetTextureFormatInfo(Format format, TextureFormatInfo& out)
{
	switch (format)
	{
	case Format::R8_UNorm:
		out = { GL_R8, GL_RED, GL_UNSIGNED_BYTE };
		return true;
	case Format::R8G8B8A8_UNorm:
		out = { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE };
		return true;
	case Format::R8G8B8A8_SRGB:
		out = { GL_SRGB8_ALPHA8, GL_RGBA, GL_UNSIGNED_BYTE };
		return true;
	case Format::R32_Float:
		out = { GL_R32F, GL_RED, GL_FLOAT };
		return true;
	case Format::D24_UNorm_S8_UInt:
		out = { GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8 };
		return true;
	case Format::D32_Float:
		out = { GL_DEPTH_COMPONENT32F, GL_DEPTH_COMPONENT, GL_FLOAT };
		return true;
	default:
		return false;
	}
}

bool GetAttributeFormat(Format format, GLint& components, GLenum& type, bool& normalized)
{
	normalized = false;
	type = GL_FLOAT;
	switch (format)
	{
	case Format::R32_Float:
		components = 1;
		return true;
	case Format::R32G32_Float:
		components = 2;
		return true;
	case Format::R32G32B32_Float:
		components = 3;
		return true;
	case Format::R32G32B32A32_Float:
		components = 4;
		return true;
	default:
		return false;
	}
}

GLenum ToGLTopology(Topology topology)
{
	switch (topology)
	{
	case Topology::LineList:
		return GL_LINES;
	case Topology::PointList:
		return GL_POINTS;
	case Topology::TriangleList:
	default:
		return GL_TRIANGLES;
	}
}

GLenum ToGLCompare(CompareOp compare)
{
	switch (compare)
	{
	case CompareOp::Never:
		return GL_NEVER;
	case CompareOp::Equal:
		return GL_EQUAL;
	case CompareOp::LessEqual:
		return GL_LEQUAL;
	case CompareOp::Greater:
		return GL_GREATER;
	case CompareOp::GreaterEqual:
		return GL_GEQUAL;
	case CompareOp::Always:
		return GL_ALWAYS;
	case CompareOp::Less:
	default:
		return GL_LESS;
	}
}

GLenum ToGLStage(ShaderStage stage)
{
	switch (stage)
	{
	case ShaderStage::Vertex:
		return GL_VERTEX_SHADER;
	case ShaderStage::Fragment:
		return GL_FRAGMENT_SHADER;
	case ShaderStage::Compute:
		return GL_COMPUTE_SHADER;
	default:
		return 0;
	}
}

class GLBuffer final : public Buffer
{
public:
	explicit GLBuffer(const BufferDesc& desc) : m_desc(desc) { }

	~GLBuffer() override
	{
		if (m_id != 0)
			glDeleteBuffers(1, &m_id);
	}

	bool Create(const void* initialData)
	{
		if (m_desc.size == 0)
			return false;

		glGenBuffers(1, &m_id);
		if (m_id == 0)
			return false;

		const GLenum target = GetTarget();
		glBindBuffer(target, m_id);
		glBufferData(target, static_cast<GLsizeiptr>(m_desc.size), initialData, m_desc.usage == BufferUsage::Uniform ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);
		glBindBuffer(target, 0);
		return true;
	}

	void Update(const void* data, uint64_t size, uint64_t offset)
	{
		if (m_id == 0 || data == nullptr || size == 0 || offset + size > m_desc.size)
			return;

		const GLenum target = GetTarget();
		glBindBuffer(target, m_id);
		glBufferSubData(target, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size), data);
		glBindBuffer(target, 0);
	}

	[[nodiscard]] uint64_t GetSize() const override
	{
		return m_desc.size;
	}

	[[nodiscard]] uintptr_t GetNativeHandle() const override
	{
		return static_cast<uintptr_t>(m_id);
	}

	[[nodiscard]] GLuint GetId() const
	{
		return m_id;
	}

	[[nodiscard]] BufferUsage GetUsage() const
	{
		return m_desc.usage;
	}

private:
	[[nodiscard]] GLenum GetTarget() const
	{
		switch (m_desc.usage)
		{
		case BufferUsage::Index:
			return GL_ELEMENT_ARRAY_BUFFER;
		case BufferUsage::Uniform:
			return GL_UNIFORM_BUFFER;
		case BufferUsage::Vertex:
		default:
			return GL_ARRAY_BUFFER;
		}
	}

	BufferDesc m_desc;
	GLuint m_id = 0;
};

class GLTexture final : public Texture
{
public:
	explicit GLTexture(const TextureDesc& desc) : m_desc(desc) { }

	~GLTexture() override
	{
		if (m_id != 0)
			glDeleteTextures(1, &m_id);
	}

	bool Create(const void* initialData)
	{
		TextureFormatInfo info;
		if (m_desc.width == 0 || m_desc.height == 0 || !GetTextureFormatInfo(m_desc.format, info))
			return false;
		if (m_desc.sampleCount == 0 || (m_desc.sampleCount > 1 && (m_desc.sampleCount & (m_desc.sampleCount - 1)) != 0))
			return false;

		const bool multisampled = m_desc.sampleCount > 1;
		if (multisampled && initialData != nullptr)
			return false;

		glGenTextures(1, &m_id);
		if (m_id == 0)
			return false;

		const GLenum target = multisampled ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
		glBindTexture(target, m_id);
		if (multisampled)
		{
			glTexImage2DMultisample(target, static_cast<GLsizei>(m_desc.sampleCount), info.internalFormat,
				static_cast<GLsizei>(m_desc.width), static_cast<GLsizei>(m_desc.height), GL_TRUE);
		}
		else
		{
			glTexImage2D(target, 0, info.internalFormat,
				static_cast<GLsizei>(m_desc.width), static_cast<GLsizei>(m_desc.height),
				0, info.format, info.type, initialData);

			const GLint minFilter = m_desc.minFilter == Filter::Nearest ? GL_NEAREST : GL_LINEAR;
			const GLint magFilter = m_desc.magFilter == Filter::Nearest ? GL_NEAREST : GL_LINEAR;
			const GLint wrap = m_desc.wrap == WrapMode::Repeat ? GL_REPEAT : GL_CLAMP_TO_EDGE;
			glTexParameteri(target, GL_TEXTURE_MIN_FILTER, minFilter);
			glTexParameteri(target, GL_TEXTURE_MAG_FILTER, magFilter);
			glTexParameteri(target, GL_TEXTURE_WRAP_S, wrap);
			glTexParameteri(target, GL_TEXTURE_WRAP_T, wrap);
		}
		glBindTexture(target, 0);

		return true;
	}

	[[nodiscard]] const TextureDesc& GetDesc() const override
	{
		return m_desc;
	}

	[[nodiscard]] uintptr_t GetNativeHandle() const override
	{
		return static_cast<uintptr_t>(m_id);
	}

	[[nodiscard]] GLuint GetId() const
	{
		return m_id;
	}

private:
	TextureDesc m_desc;
	GLuint m_id = 0;
};

class GLRenderTarget final : public RenderTarget
{
public:
	explicit GLRenderTarget(const RenderTargetDesc& desc) : m_desc(desc) { }

	~GLRenderTarget() override
	{
		if (m_resolveFramebuffer != 0)
			glDeleteFramebuffers(1, &m_resolveFramebuffer);
		if (m_framebuffer != 0)
			glDeleteFramebuffers(1, &m_framebuffer);
	}

	bool Create()
	{
		if (m_desc.width == 0 || m_desc.height == 0)
			return false;
		if (m_desc.sampleCount == 0 || (m_desc.sampleCount > 1 && (m_desc.sampleCount & (m_desc.sampleCount - 1)) != 0))
			return false;

		TextureDesc colorDesc;
		colorDesc.width = m_desc.width;
		colorDesc.height = m_desc.height;
		colorDesc.format = m_desc.colorFormat;
		colorDesc.usage = TextureUsage::RenderTarget | TextureUsage::Sampled;
		colorDesc.sampleCount = m_desc.sampleCount;
		colorDesc.debugName = m_desc.debugName;

		m_color = std::make_unique<GLTexture>(colorDesc);
		if (!m_color->Create(nullptr))
			return false;

		if (m_desc.depthFormat != Format::Undefined)
		{
			TextureDesc depthDesc;
			depthDesc.width = m_desc.width;
			depthDesc.height = m_desc.height;
			depthDesc.format = m_desc.depthFormat;
			depthDesc.usage = TextureUsage::DepthStencil;
			depthDesc.sampleCount = m_desc.sampleCount;
			depthDesc.debugName = m_desc.debugName;

			m_depth = std::make_unique<GLTexture>(depthDesc);
			if (!m_depth->Create(nullptr))
				return false;
		}

		if (m_desc.sampleCount > 1)
		{
			TextureDesc resolveDesc = colorDesc;
			resolveDesc.sampleCount = 1;

			m_resolve = std::make_unique<GLTexture>(resolveDesc);
			if (!m_resolve->Create(nullptr))
				return false;
		}

		const GLenum attachmentTarget = m_desc.sampleCount > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;

		glGenFramebuffers(1, &m_framebuffer);
		if (m_framebuffer == 0)
			return false;

		glBindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, attachmentTarget, m_color->GetId(), 0);

		if (m_depth)
		{
			const GLenum attachment = m_desc.depthFormat == Format::D24_UNorm_S8_UInt
				? GL_DEPTH_STENCIL_ATTACHMENT
				: GL_DEPTH_ATTACHMENT;
			glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, attachmentTarget, m_depth->GetId(), 0);
		}

		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			return false;
		}

		if (m_resolve)
		{
			glGenFramebuffers(1, &m_resolveFramebuffer);
			if (m_resolveFramebuffer == 0)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				return false;
			}

			glBindFramebuffer(GL_FRAMEBUFFER, m_resolveFramebuffer);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_resolve->GetId(), 0);
			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				return false;
			}
		}

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		return true;
	}

	[[nodiscard]] const RenderTargetDesc& GetDesc() const override
	{
		return m_desc;
	}

	Texture& GetColorTexture() override
	{
		return *m_color;
	}

	Texture* GetDepthTexture() override
	{
		return m_depth.get();
	}

	Texture* GetResolveTexture() override
	{
		return m_resolve.get();
	}

	bool Resolve() override
	{
		if (m_desc.sampleCount <= 1)
			return true;
		if (m_framebuffer == 0 || m_resolveFramebuffer == 0)
			return false;

		GLint previousRead = 0;
		GLint previousDraw = 0;
		glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &previousRead);
		glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &previousDraw);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, m_framebuffer);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_resolveFramebuffer);
		glBlitFramebuffer(0, 0, static_cast<GLint>(m_desc.width), static_cast<GLint>(m_desc.height),
			0, 0, static_cast<GLint>(m_desc.width), static_cast<GLint>(m_desc.height),
			GL_COLOR_BUFFER_BIT, GL_LINEAR);

		glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(previousRead));
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(previousDraw));
		return true;
	}

	[[nodiscard]] uintptr_t GetNativeHandle() const override
	{
		return static_cast<uintptr_t>(m_framebuffer);
	}

private:
	RenderTargetDesc m_desc;
	std::unique_ptr<GLTexture> m_color;
	std::unique_ptr<GLTexture> m_depth;
	std::unique_ptr<GLTexture> m_resolve;
	GLuint m_framebuffer = 0;
	GLuint m_resolveFramebuffer = 0;
};

class GLShader final : public Shader
{
public:
	explicit GLShader(const ShaderDesc& desc) : m_desc(desc) { }

	~GLShader() override
	{
		if (m_id != 0)
			glDeleteShader(m_id);
	}

	bool Create()
	{
		if (m_desc.glslSource == nullptr || (m_desc.stage != ShaderStage::Vertex && m_desc.stage != ShaderStage::Fragment))
			return false;

		const GLenum stage = ToGLStage(m_desc.stage);
		if (stage == 0)
			return false;

		m_id = glCreateShader(stage);
		if (m_id == 0)
			return false;

		glShaderSource(m_id, 1, &m_desc.glslSource, nullptr);
		glCompileShader(m_id);

		GLint compiled = GL_FALSE;
		glGetShaderiv(m_id, GL_COMPILE_STATUS, &compiled);
		if (compiled != GL_TRUE)
		{
			glDeleteShader(m_id);
			m_id = 0;
			return false;
		}
		return true;
	}

	[[nodiscard]] ShaderStage GetStage() const override
	{
		return m_desc.stage;
	}

	[[nodiscard]] GLuint GetId() const
	{
		return m_id;
	}

private:
	ShaderDesc m_desc;
	GLuint m_id = 0;
};

class GLPipeline final : public Pipeline
{
public:
	explicit GLPipeline(const PipelineDesc& desc) : m_desc(desc) { }

	~GLPipeline() override
	{
		if (m_program != 0)
			glDeleteProgram(m_program);
	}

	bool Create()
	{
		if (m_desc.vertexShader == nullptr || m_desc.fragmentShader == nullptr)
			return false;

		const GLuint vertexShader = static_cast<GLShader*>(m_desc.vertexShader)->GetId();
		const GLuint fragmentShader = static_cast<GLShader*>(m_desc.fragmentShader)->GetId();
		if (vertexShader == 0 || fragmentShader == 0)
			return false;

		m_program = glCreateProgram();
		if (m_program == 0)
			return false;

		glAttachShader(m_program, vertexShader);
		glAttachShader(m_program, fragmentShader);
		glLinkProgram(m_program);

		GLint linked = GL_FALSE;
		glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
		if (linked != GL_TRUE)
		{
			glDeleteProgram(m_program);
			m_program = 0;
			return false;
		}
		return true;
	}

	[[nodiscard]] GLuint GetProgram() const
	{
		return m_program;
	}

	[[nodiscard]] const PipelineDesc& GetDesc() const
	{
		return m_desc;
	}

private:
	PipelineDesc m_desc;
	GLuint m_program = 0;
};

class GLCommandList final : public CommandList
{
public:
	GLCommandList() = default;

	~GLCommandList() override
	{
		if (m_vao != 0)
			glDeleteVertexArrays(1, &m_vao);
		if (m_passFramebuffer != 0)
			glDeleteFramebuffers(1, &m_passFramebuffer);
	}

	void Begin() override
	{
		if (m_vao == 0)
			glGenVertexArrays(1, &m_vao);
		glBindVertexArray(m_vao);
	}

	void End() override
	{
		glBindVertexArray(0);
	}

	void BeginRenderPass(const RenderPassDesc& desc) override
	{
		m_inPass = false;

		if (desc.colorTarget != nullptr)
		{
			GLTexture& color = static_cast<GLTexture&>(*desc.colorTarget);
			if (m_passFramebuffer == 0)
				glGenFramebuffers(1, &m_passFramebuffer);

			glBindFramebuffer(GL_FRAMEBUFFER, m_passFramebuffer);
			const GLenum colorTargetType = color.GetDesc().sampleCount > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, colorTargetType, color.GetId(), 0);

			if (desc.depthTarget != nullptr)
			{
				GLTexture& depth = static_cast<GLTexture&>(*desc.depthTarget);
				const GLenum attachment = depth.GetDesc().format == Format::D24_UNorm_S8_UInt ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT;
				const GLenum depthTargetType = depth.GetDesc().sampleCount > 1 ? GL_TEXTURE_2D_MULTISAMPLE : GL_TEXTURE_2D;
				glFramebufferTexture2D(GL_FRAMEBUFFER, attachment, depthTargetType, depth.GetId(), 0);
			}

			if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
			{
				glBindFramebuffer(GL_FRAMEBUFFER, 0);
				return;
			}

			Viewport viewport;
			viewport.width = static_cast<float>(color.GetDesc().width);
			viewport.height = static_cast<float>(color.GetDesc().height);
			SetViewport(viewport);
		}
		else
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
		}

		GLbitfield clearMask = 0;
		if (desc.colorLoadOp == LoadOp::Clear)
		{
			glClearColor(desc.clear.r, desc.clear.g, desc.clear.b, desc.clear.a);
			clearMask |= GL_COLOR_BUFFER_BIT;
		}
		if (desc.depthLoadOp == LoadOp::Clear)
		{
			glClearDepth(desc.clear.depth);
			glClearStencil(static_cast<GLint>(desc.clear.stencil));
			clearMask |= GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
		}
		if (clearMask != 0)
			glClear(clearMask);

		m_inPass = true;
	}

	void EndRenderPass() override
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		m_inPass = false;
	}

	void SetViewport(const Viewport& viewport) override
	{
		glViewport(static_cast<GLint>(viewport.x), static_cast<GLint>(viewport.y),
			static_cast<GLsizei>(viewport.width), static_cast<GLsizei>(viewport.height));
		glDepthRange(viewport.minDepth, viewport.maxDepth);
	}

	void BindPipeline(Pipeline& pipeline) override
	{
		GLPipeline& glPipeline = static_cast<GLPipeline&>(pipeline);
		m_pipeline = &glPipeline;
		const PipelineDesc& desc = glPipeline.GetDesc();

		glUseProgram(glPipeline.GetProgram());

		if (desc.depthTest)
			glEnable(GL_DEPTH_TEST);
		else
			glDisable(GL_DEPTH_TEST);
		glDepthMask(desc.depthWrite ? GL_TRUE : GL_FALSE);
		glDepthFunc(ToGLCompare(desc.depthCompare));

		if (desc.cullMode == CullMode::None)
		{
			glDisable(GL_CULL_FACE);
		}
		else
		{
			glEnable(GL_CULL_FACE);
			glCullFace(desc.cullMode == CullMode::Front ? GL_FRONT : GL_BACK);
		}
		glFrontFace(desc.frontFace == FrontFace::Clockwise ? GL_CW : GL_CCW);

		if (desc.blendMode == BlendMode::AlphaBlend)
		{
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		else
		{
			glDisable(GL_BLEND);
		}

		ApplyVertexLayout();
	}

	void BindVertexBuffer(Buffer& buffer) override
	{
		GLBuffer& glBuffer = static_cast<GLBuffer&>(buffer);
		if (glBuffer.GetUsage() != BufferUsage::Vertex)
			return;

		m_vertexBuffer = &glBuffer;
		glBindVertexArray(m_vao);
		glBindBuffer(GL_ARRAY_BUFFER, glBuffer.GetId());
		ApplyVertexLayout();
	}

	void BindIndexBuffer(Buffer& buffer) override
	{
		GLBuffer& glBuffer = static_cast<GLBuffer&>(buffer);
		if (glBuffer.GetUsage() != BufferUsage::Index)
			return;

		glBindVertexArray(m_vao);
		glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, glBuffer.GetId());
	}

	void BindUniformBuffer(uint32_t slot, Buffer& buffer, uint64_t offset, uint64_t size) override
	{
		const GLuint id = static_cast<GLBuffer&>(buffer).GetId();
		if (size > 0)
			glBindBufferRange(GL_UNIFORM_BUFFER, slot, id, static_cast<GLintptr>(offset), static_cast<GLsizeiptr>(size));
		else
			glBindBufferBase(GL_UNIFORM_BUFFER, slot, id);
	}

	void BindTexture(uint32_t slot, Texture& texture) override
	{
		glActiveTexture(GL_TEXTURE0 + slot);
		glBindTexture(GL_TEXTURE_2D, static_cast<GLTexture&>(texture).GetId());
	}

	void Draw(uint32_t vertexCount, uint32_t firstVertex) override
	{
		if (!m_inPass || m_pipeline == nullptr)
			return;
		glDrawArrays(ToGLTopology(m_pipeline->GetDesc().topology), static_cast<GLint>(firstVertex), static_cast<GLsizei>(vertexCount));
	}

	void DrawIndexed(uint32_t indexCount, uint32_t firstIndex) override
	{
		if (!m_inPass || m_pipeline == nullptr)
			return;

		const void* offset = reinterpret_cast<const void*>(static_cast<uintptr_t>(firstIndex) * sizeof(uint32_t));
		glDrawElements(ToGLTopology(m_pipeline->GetDesc().topology), static_cast<GLsizei>(indexCount), GL_UNSIGNED_INT, offset);
	}

private:
	void ApplyVertexLayout()
	{
		if (m_pipeline == nullptr || m_vertexBuffer == nullptr)
			return;

		glBindVertexArray(m_vao);
		for (GLuint location = 0; location < 16; ++location)
			glDisableVertexAttribArray(location);

		const VertexLayout& layout = m_pipeline->GetDesc().vertexLayout;
		for (const VertexAttribute& attribute : layout.attributes)
		{
			GLint components = 0;
			GLenum type = 0;
			bool normalized = false;
			if (!GetAttributeFormat(attribute.format, components, type, normalized))
				continue;

			glEnableVertexAttribArray(attribute.location);
			glVertexAttribPointer(attribute.location, components, type, normalized ? GL_TRUE : GL_FALSE,
				static_cast<GLsizei>(layout.stride),
				reinterpret_cast<const void*>(static_cast<uintptr_t>(attribute.offset)));
		}
	}

	GLuint m_vao = 0;
	GLuint m_passFramebuffer = 0;
	GLPipeline* m_pipeline = nullptr;
	GLBuffer* m_vertexBuffer = nullptr;
	bool m_inPass = false;
};

class OpenGLDevice final : public Device
{
public:
	OpenGLDevice()
	{
		const char* deviceName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
		const char* apiVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));
		m_deviceName = deviceName != nullptr ? deviceName : "OpenGL";
		m_apiVersion = apiVersion != nullptr ? apiVersion : "";

		GLint maxTextureSize = 0;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
		m_maxTextureSize = static_cast<int>(maxTextureSize);
	}

	[[nodiscard]] Backend GetBackend() const override
	{
		return Backend::OpenGL;
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

	std::unique_ptr<Buffer> CreateBuffer(const BufferDesc& desc, const void* initialData) override
	{
		auto buffer = std::make_unique<GLBuffer>(desc);
		if (!buffer->Create(initialData))
			return nullptr;
		return buffer;
	}

	void UpdateBuffer(Buffer& buffer, const void* data, uint64_t size, uint64_t offset) override
	{
		static_cast<GLBuffer&>(buffer).Update(data, size, offset);
	}

	std::unique_ptr<Texture> CreateTexture(const TextureDesc& desc, const void* initialData) override
	{
		auto texture = std::make_unique<GLTexture>(desc);
		if (!texture->Create(initialData))
			return nullptr;
		return texture;
	}

	std::unique_ptr<RenderTarget> CreateRenderTarget(const RenderTargetDesc& desc) override
	{
		auto target = std::make_unique<GLRenderTarget>(desc);
		if (!target->Create())
			return nullptr;
		return target;
	}

	std::unique_ptr<Shader> CreateShader(const ShaderDesc& desc) override
	{
		auto shader = std::make_unique<GLShader>(desc);
		if (!shader->Create())
			return nullptr;
		return shader;
	}

	std::unique_ptr<Pipeline> CreatePipeline(const PipelineDesc& desc) override
	{
		auto pipeline = std::make_unique<GLPipeline>(desc);
		if (!pipeline->Create())
			return nullptr;
		return pipeline;
	}

	std::unique_ptr<CommandList> CreateCommandList() override
	{
		return std::make_unique<GLCommandList>();
	}

	void Submit(CommandList& commandList) override
	{
		(void)commandList;
	}

	void WaitIdle() override
	{
		glFinish();
	}

	bool ReadTexture(Texture& texture, void* outData, size_t outSize) override
	{
		GLTexture& glTexture = static_cast<GLTexture&>(texture);
		const TextureDesc& desc = glTexture.GetDesc();
		if (desc.format != Format::R8G8B8A8_UNorm || outData == nullptr)
			return false;
		if (outSize < static_cast<size_t>(desc.width) * desc.height * 4u)
			return false;

		GLuint framebuffer = 0;
		glGenFramebuffers(1, &framebuffer);
		glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
		glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, glTexture.GetId(), 0);
		if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
		{
			glBindFramebuffer(GL_FRAMEBUFFER, 0);
			glDeleteFramebuffers(1, &framebuffer);
			return false;
		}

		glPixelStorei(GL_PACK_ALIGNMENT, 1);
		glReadPixels(0, 0, static_cast<GLsizei>(desc.width), static_cast<GLsizei>(desc.height), GL_RGBA, GL_UNSIGNED_BYTE, outData);

		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &framebuffer);
		return true;
	}

private:
	std::string m_deviceName;
	std::string m_apiVersion;
	int m_maxTextureSize = 0;
};

} // namespace

std::unique_ptr<Device> CreateOpenGLDevice()
{
	return std::make_unique<OpenGLDevice>();
}

} // namespace bones::rhi

#endif

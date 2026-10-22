// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include "texture_default.h"

#if defined(_WIN32) || defined(_WIN64) || defined(__LINUX__)

#include <malloc.h>
#include <string>
//#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
//#define STB_IMAGE_RESIZE_IMPLEMENTATION
#include <stb_image_resize.h>


#if defined(_WIN32) || defined(_WIN64) || defined(__LINUX__)
#include <thread>
#include <glad/gl.h>
#endif

#include <engine/engine.h>
#include <engine/debug/debug.h>
#include <engine/asset_management/asset_manager.h>
#include <engine/file_system/file.h>
#include <engine/tools/internal_math.h>
#include <engine/file_system/async_file_loading.h>
#include <engine/debug/memory_tracker.h>
#include <engine/asset_management/project_manager.h>
#include <engine/debug/performance.h>
#include <engine/debug/stack_debug_object.h>
#include <engine/graphics/renderer/renderer.h>


TextureDefault::~TextureDefault()
{
	this->UnloadFileReference();
}

void TextureDefault::ReleaseTexture()
{
	if (!m_texture)
		return;

	m_texture.reset();
#if defined (DEBUG)
	Performance::s_textureMemoryTracker->Deallocate(m_width * height * 4);
#endif
}

void TextureDefault::Bind() const
{
	XASSERT(Engine::IsCalledFromMainThread(), "Function called from another thread");

	const unsigned int textureId = GetTextureId();
	if (textureId == 0)
		return;

	glBindTexture(GL_TEXTURE_2D, textureId);
	ApplyTextureFilters();
}

int TextureDefault::GetWrapModeEnum(WrapMode wrapMode) const
{
	int mode = GL_REPEAT;
	switch (wrapMode)
	{
	case WrapMode::ClampToEdge:
		mode = GL_CLAMP_TO_EDGE;
		break;
	case WrapMode::Repeat:
		mode = GL_REPEAT;
		break;
	}
	return mode;
}

void TextureDefault::ApplyTextureFilters() const
{
	XASSERT(Engine::IsCalledFromMainThread(), "Function called from another thread");

	// Get the right filter depending of the texture settings
	int minFilterValue = GL_LINEAR;
	int magfilterValue = GL_LINEAR;
	if (GetFilter() == Filter::Bilinear)
	{
		if (GetUseMipmap())
		{
			minFilterValue = GL_LINEAR_MIPMAP_LINEAR;
		}
		else
		{
			minFilterValue = GL_LINEAR;
		}
		magfilterValue = GL_LINEAR;
	}
	else if (GetFilter() == Filter::Point)
	{
		if (GetUseMipmap())
		{
			minFilterValue = GL_NEAREST_MIPMAP_NEAREST;
		}
		else
		{
			minFilterValue = GL_NEAREST;
		}
		magfilterValue = GL_NEAREST;
	}

	const int wrap = GetWrapModeEnum(GetWrapMode());
	
	// Apply filters
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, minFilterValue);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, magfilterValue);
}

void TextureDefault::OnLoadFileReferenceFinished()
{
	STACK_DEBUG_OBJECT(STACK_HIGH_PRIORITY);

	// The loading may have failed (missing, empty or invalid file)
	if (!m_buffer || m_fileStatus == FileStatus::FileStatus_Failed)
	{
		return;
	}

	SetData(m_buffer);

	free(m_buffer);
	// Avoid a double free/use after free if the texture is reloaded and the loading fails
	m_buffer = nullptr;
}

// TODO: This function only supports 1 color textures, add enum for texture color type
void TextureDefault::SetData(const unsigned char *texData)
{
	XASSERT(Engine::IsCalledFromMainThread(), "Function called from another thread");

	STACK_DEBUG_OBJECT(STACK_HIGH_PRIORITY);

	XASSERT(texData != nullptr, "[TextureDefault::SetTextureLevel] texData is nullptr");

	bones::rhi::Device* device = Engine::GetRenderer().GetRhiDevice();
	if (!device)
	{
		Debug::PrintError("[TextureDefault] Cannot create a texture without an RHI device", true);
		return;
	}

	ReleaseTexture();

	bones::rhi::TextureDesc desc;
	desc.width = static_cast<uint32_t>(GetWidth());
	desc.height = static_cast<uint32_t>(GetHeight());
	desc.format = bones::rhi::Format::R8G8B8A8_UNorm;
	desc.usage = bones::rhi::TextureUsage::Sampled;
	desc.minFilter = GetFilter() == Filter::Bilinear ? bones::rhi::Filter::Linear : bones::rhi::Filter::Nearest;
	desc.magFilter = GetFilter() == Filter::Bilinear ? bones::rhi::Filter::Linear : bones::rhi::Filter::Nearest;
	desc.wrap = GetWrapMode() == WrapMode::Repeat ? bones::rhi::WrapMode::Repeat : bones::rhi::WrapMode::ClampToEdge;

	m_texture = device->CreateTexture(desc, texData);
	if (!m_texture)
	{
		Debug::PrintError("[TextureDefault] Cannot create the RHI texture", true);
		return;
	}

#if defined (DEBUG)
	Performance::s_textureMemoryTracker->Allocate(m_width * height * 4);
#endif

	Bind();

	// The RHI contract does not expose mip generation yet. The legacy OpenGL
	// path still generates them through the native texture handle.
	if (GetUseMipmap())
	{
		glGenerateMipmap(GL_TEXTURE_2D);
	}
	ApplyTextureFilters();

	isValid = true;
	m_fileStatus = FileStatus::FileStatus_Loaded;
}

void TextureDefault::Unload()
{
	STACK_DEBUG_OBJECT(STACK_HIGH_PRIORITY);

	ClearSpriteSelections();
	ReleaseTexture();
}

#endif
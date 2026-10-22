// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#pragma once

#include <engine/graphics/rhi/rhi.h>

#include <memory>

#if defined(BONES_VULKAN)

namespace bones::rhi
{

/**
* @brief Create a Vulkan implementation of the RHI.
*
* The device is headless: it does not create a window, a surface or a
* swapchain. Rendering goes to textures created with TextureUsage::RenderTarget.
*
* Current limits:
* - Sampled textures need initial data; render targets are not yet transitioned
*   for sampling.
* - Descriptor sets are allocated per draw (up to 1024 draws per command list).
* - Submission is synchronous and single-threaded.
* - Only RGBA8 textures can be uploaded and read back.
*/
std::unique_ptr<Device> CreateVulkanDevice();

} // namespace bones::rhi

#endif

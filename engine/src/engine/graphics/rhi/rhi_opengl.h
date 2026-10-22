// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#pragma once

#include <engine/graphics/rhi/rhi.h>

#include <memory>

#if !defined(__PS3__)

namespace bones::rhi
{

/**
* @brief Create an OpenGL implementation of the RHI.
*
* The caller must have a current OpenGL context. The device does not own the
* context and does not create a window.
*/
std::unique_ptr<Device> CreateOpenGLDevice();

} // namespace bones::rhi

#endif

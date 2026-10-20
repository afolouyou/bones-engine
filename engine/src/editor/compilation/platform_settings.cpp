// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#include "platform_settings.h"

#include <engine/graphics/texture/texture.h>

int PlatformSettingsWindows::IsValid()
{
	return static_cast<int>(PlatformSettingsErrorWindows::None); // Valid
}

int PlatformSettingsPS3::IsValid()
{
	return static_cast<int>(PlatformSettingsErrorPS3::None); // Valid
}

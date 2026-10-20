// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

#pragma once

#include <engine/reflection/enum_utils.h>

ENUM(Platform,
	P_Windows,
	P_Linux,
	P_PS3,
	P_COUNT);

ENUM(AssetPlatform,
	AP_Standalone,
	AP_PS3,
	AP_COUNT);

static const char* s_assetPlatformNames[static_cast<int>(AssetPlatform::AP_COUNT)] = {
	"Standalone",
	"PS3"
};
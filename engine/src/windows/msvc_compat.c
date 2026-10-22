// SPDX-License-Identifier: MIT
//
// Copyright (c) 2022-2026 Gregory Machefer (Fewnity)
//
// This file is part of Bones Engine

// The curl and zlib archives in third_party/libs are MSVC builds, while a
// MinGW target supplies the C runtime. MSVC emits a handful of compiler
// runtime symbols that MinGW does not define. Provide them here so those
// archives can be linked without pulling in the MSVC CRT.
//
// Only the MinGW target needs this. MSVC targets get the real implementations
// from the Visual C++ runtime.

#if defined(_WIN32) && !defined(_MSC_VER)

#include <stdint.h>
#include <stdlib.h>

// Referenced by MSVC-compiled code that uses floating point.
int _fltused = 1;

// Stack cookie used by the /GS checks emitted into the MSVC archives.
uintptr_t __security_cookie = 0x00002B992DDFA232ULL;

void __security_check_cookie(uintptr_t cookie)
{
	if (cookie != __security_cookie)
		abort();
}

// Called from the unwind information of functions with a GS handler. Curl and
// zlib are C code and do not throw, so there is nothing to restore here.
void __GSHandlerCheck(void)
{
}

void __report_rangecheckfailure(void)
{
	abort();
}

#endif

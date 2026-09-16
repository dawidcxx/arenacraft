// Vendored-ACE platform configuration, selected at build time by the
// C preprocessor so a single committed file works for every target.
//
// This file satisfies the `#include "ace/config.h"` performed by
// ace/config-macros.h (the directory containing `ace/` is put on the
// include path by the zig build in ./Deps.zig).

#pragma once

// We link ACE as a static library; this must be defined before
// ace/ACE_export.h is processed so ACE_HAS_DLL resolves to 0 and
// ACE_Export expands to nothing (see ACE_export.h).
#ifndef ACE_AS_STATIC_LIBS
#define ACE_AS_STATIC_LIBS 1
#endif

#if defined(_WIN32)
#include "ace/config-win32.h"
#elif defined(__APPLE__)
#include "ace/config-macosx.h"
#elif defined(__linux__)
#include "ace/config-linux.h"
#elif defined(__FreeBSD__)
#include "ace/config-freebsd.h"
#elif defined(__NetBSD__)
#include "ace/config-netbsd.h"
#elif defined(__OpenBSD__)
#include "ace/config-openbsd.h"
#else
#error "ACE: no platform config selected - extend zig-build/ace-config/ace/config.h"
#endif

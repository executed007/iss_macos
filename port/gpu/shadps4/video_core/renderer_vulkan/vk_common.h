// SPDX-FileCopyrightText: Copyright 2024 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Include vulkan-hpp header
#define VK_ENABLE_BETA_EXTENSIONS
#define VK_NO_PROTOTYPES
#define VULKAN_HPP_DISPATCH_LOADER_DYNAMIC 1
#define VULKAN_HPP_NO_CONSTRUCTORS
#define VULKAN_HPP_NO_STRUCT_SETTERS
#define VULKAN_HPP_HAS_SPACESHIP_OPERATOR
#define VULKAN_HPP_NO_EXCEPTIONS
// Define assert-on-result to nothing to instead return the result for our handling.
#define VULKAN_HPP_ASSERT_ON_RESULT

#if defined(__APPLE__) && !defined(NDEBUG)
// tlg: DispatchLoaderBase has debug-only members, so a -UNDEBUG test TU laid out the dispatcher
// 16 bytes off the library's, and ld64 merged the test's inline init() into the library's calls
// (crash in defaultDispatchLoaderDynamic.init). Same layout everywhere; assert() re-armed below.
#define NDEBUG
#define TLG_VK_REARM_ASSERT
#endif
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-value"
#include <vulkan/vulkan.hpp>
#pragma clang diagnostic pop
#ifdef TLG_VK_REARM_ASSERT
#undef NDEBUG
#undef TLG_VK_REARM_ASSERT
#include <assert.h>
#endif

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

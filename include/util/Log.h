// =============================================================================
//  PetPal - Log.h
//  Lightweight logging that compiles to nothing in release builds. On the 3DS,
//  output goes to the gdb/3dslink console when a debugger is attached. Use the
//  PP_LOG* macros; never call printf directly from gameplay code.
// =============================================================================
#pragma once

#include <cstdio>

#ifndef PETPAL_DEBUG
#define PETPAL_DEBUG 0
#endif

#if PETPAL_DEBUG
    #define PP_LOG(...)   do { std::printf("[PetPal] "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)
    #define PP_WARN(...)  do { std::printf("[PetPal][warn] "); std::printf(__VA_ARGS__); std::printf("\n"); } while (0)
    #define PP_ERR(...)   do { std::printf("[PetPal][err] ");  std::printf(__VA_ARGS__); std::printf("\n"); } while (0)
#else
    #define PP_LOG(...)   do {} while (0)
    #define PP_WARN(...)  do {} while (0)
    #define PP_ERR(...)   do {} while (0)
#endif

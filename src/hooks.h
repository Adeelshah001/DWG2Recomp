#pragma once

#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/system/function_dispatcher.h>

// Match the pinned SDK's big-endian guest memory accessors when compiled
// standalone (without dwg2_pch.h).
#ifndef REX_LOAD_U32
#if REX_PLATFORM_WIN32 || (REX_PLATFORM_MAC && REX_ARCH_ARM64)
#define DWG2_PHYS_OFFSET(x) ((uint32_t(x) >= 0xE0000000u) ? 0x1000u : 0u)
#else
#define DWG2_PHYS_OFFSET(x) 0u
#endif
#define REX_LOAD_U32(x) \
  __builtin_bswap32(*(volatile uint32_t*)(base + uint32_t(x) + DWG2_PHYS_OFFSET(x)))
#define REX_STORE_U32(x, y) \
  (*(volatile uint32_t*)(base + uint32_t(x) + DWG2_PHYS_OFFSET(x)) = __builtin_bswap32(y))
#endif

#ifndef DWG2_PHYS_OFFSET
#define DWG2_PHYS_OFFSET(x) REX_PHYS_HOST_OFFSET(x)
#endif

REXCVAR_DECLARE(bool, dwg2_diag_trace);

#ifdef DWG2_PRIVATE_HOOKS
namespace rex {
class Runtime;
namespace ui {
class Window;
}
}
#endif

namespace dwg2::hooks {

void RegisterBootHooks(rex::runtime::FunctionDispatcher* dispatcher);
void RegisterSaveLoadHooks(rex::runtime::FunctionDispatcher* dispatcher);
void RegisterCameraFovHooks(rex::runtime::FunctionDispatcher* dispatcher);
void RegisterTimingFpsHooks(rex::runtime::FunctionDispatcher* dispatcher);
#ifdef DWG2_PRIVATE_HOOKS
void RegisterPrivateHooks(rex::Runtime* runtime, rex::ui::Window* window);
bool StartupStorageWindow();
#endif

}  // namespace dwg2::hooks

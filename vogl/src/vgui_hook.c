/*
 * vgui_hook.c - trampoline for the VGUI device driver in the Windows
 * shared-library build of vogl.
 *
 * drivers.c (compiled with -DVGUI) calls _VGUI_devcpy() to install the VGUI
 * device table into the global vdevice. In the static build -- and on ELF,
 * where a shared library may leave symbols undefined -- _VGUI_devcpy() is
 * supplied by the driver object (vogl_vgui or dummy_vogl_vgui) linked into the
 * executable and resolved by the linker/loader.
 *
 * A PE/Windows DLL cannot have undefined symbols, and a single libvogl.dll is
 * shared by several executables that each need a *different* driver (the real
 * GTK/cairo driver in avg_q_ui, the no-op dummy everywhere else). libvogl.dll
 * therefore provides _VGUI_devcpy() itself, as a trampoline that forwards to a
 * function-pointer hook. Each executable registers its driver at startup
 * through vogl_set_vgui_devcpy() from a constructor in its driver object, which
 * is pulled in unconditionally as a CMake OBJECT library.
 */
#include "vogl.h"

static void (*vogl_vgui_devcpy_hook)(void);

void
vogl_set_vgui_devcpy(void (*fn)(void)) {
	vogl_vgui_devcpy_hook = fn;
}

void
_VGUI_devcpy(void) {
	if (vogl_vgui_devcpy_hook) (*vogl_vgui_devcpy_hook)();
}

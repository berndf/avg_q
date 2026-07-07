/*
 * Copyright (C) 1998,2010,2026 Bernd Feige
 * This file is part of avg_q and released under the GPL v3 (see avg_q/COPYING).
 */
#include "vogl.h"

static void
VGUI_devcpy_impl(void) {
}

/*
 * _VGUI_devcpy
 *
 * copy the pc device into vdevice.dev. (as listed in drivers.c)
 *
 * In the Windows shared-library build, libvogl.dll provides _VGUI_devcpy()
 * as a trampoline forwarding to a hook set by vogl_set_vgui_devcpy(). This
 * object is pulled whole into the executable (OBJECT library) and registers
 * its implementation -- here the no-op dummy -- via a constructor so the
 * correct driver is selected at runtime. In the static/ELF build,
 * _VGUI_devcpy() is defined directly and resolved by the linker from this
 * object.
 */
#ifdef VOGL_VGUI_USE_HOOK
__attribute__((constructor))
static void
vogl_vgui_register(void) {
	vogl_set_vgui_devcpy(VGUI_devcpy_impl);
}
#else
void
_VGUI_devcpy(void) {
	VGUI_devcpy_impl();
}
#endif

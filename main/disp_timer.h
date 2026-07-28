// disp_timer.h — Lazy/delayed display update queue so screen refreshes don't block
// the main-loop button polling. The pattern:
//   1. Code that needs a draw calls deferDraw() instead of drawXxxLayout()
//   2. deferDraw() sets a flag and optionally forces full refresh
//   3. flushDisplayIfNeeded() (called once per loop after BLE/GPS/button work) does the actual blocking refresh
// This ensures button detection completes before any ~0.8-2.5s display update blocks the CPU.

#ifndef DISP_TIMER_H
#define DISP_TIMER_H

#include <Arduino.h>

// Call instead of drawXxxLayout() when you want a lazy (non-blocking) screen update
void deferDraw(bool force_full_refresh = false);

// Flush any pending deferred draw — call once per loop after button/BLE/GPS work
void flushDisplayIfNeeded();

// Force immediate draw (blocking) — use sparingly, e.g. boot sequence
void forceImmediateDraw();

// Tell the system that a screen update is currently in progress
bool isDisplayBusy();

#endif

// disp_timer.cpp — Lazy/delayed display update queue
//
// Problem: renderPageLoop() calls display->refresh() which blocks 0.8-2.5s, during which
// button polling stops entirely. A user holding MODE_PIN for >10s to see a long press is
// actually getting the full-refresh time added to their hold duration.
//
// Solution: two-level deferral:
//   Level 1 (mode switch): defer render until AFTER button release — button state captured first
//   Level 2 (dirty-draw): queue draw flag, flush at end of loop after all I/O done
// This ensures button detection never blocks mid-loop.

#include "disp_timer.h"
#include "display_layout.h"
#include "app_modes.h"
#include "disp_refresh.h"

extern void forceFullRefresh();

// Pending deferred draw state
static bool s_pending_draw = false;
static bool s_force_full = false;
static bool s_immediate_mode = false;
static bool s_display_busy = false;

void deferDraw(bool force_full_refresh) {
    if (s_immediate_mode) {
        if (force_full_refresh) forceFullRefresh();
        return;
    }
    s_pending_draw = true;
    s_force_full = force_full_refresh;
}

void flushDisplayIfNeeded() {
    if (s_display_busy) return;

    // Check for dirty screen from app_modes or pending draw from deferDraw
    if (!s_pending_draw && !layout_state._dirty) {
        layout_state._dirty = false;
        return;
    }

    s_display_busy = true;
    bool full = s_force_full;
    s_pending_draw = false;

    if (full) forceFullRefresh();

    // Re-render based on current mode from layout state
    const char* mode = current_mode;
    if (strcmp(mode, "RANGE") == 0) drawRangeLayout();
    else if (strcmp(mode, "BEACON") == 0) drawBeaconLayout();
    else if (strcmp(mode, "PTT") == 0) drawPttLayout();
    else if (strcmp(mode, "SCAN") == 0) drawScanLayout();
    else if (strcmp(mode, "TXT") == 0) drawTxtSingleLayout();
    else if (strcmp(mode, "TST") == 0) drawTstLayout();
    else if (strcmp(mode, "PONG") == 0) drawPongLayout();
    else if (strcmp(mode, "RAW") == 0) drawRawLayout();
    else if (strcmp(mode, "WP") == 0) drawWpLayout();
    else drawDefaultLayout();

    s_display_busy = false;
}

void forceImmediateDraw() { s_immediate_mode = true; }
void restoreDeferredDraw() { s_immediate_mode = false; }
bool isDisplayBusy() { return s_display_busy; }

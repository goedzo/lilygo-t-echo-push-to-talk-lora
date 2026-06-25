// ---- HardFault / crash detection and debugging ----
#define CRASH_MAGIC        0x43524153  // "CRAS"
#define STACK_GUARD_PAT    0xDEADBEEF
#define MAX_CRASH_LOG      16

typedef struct {
    uint32_t    magic;
    uint32_t    stack_guard_pattern;
    uint32_t    msp_before_crash;
    uint32_t    psp_main;
    uint32_t    psp_isr;
    uint32_t    lr_at_crash;
    uint32_t    cfsr;
    uint32_t    hfsr;
    uint32_t    bfar;
    uint32_t    afsr;
    uint32_t    xPSR;
    uint32_t    r0, r1, r2, r3;
    uint32_t    r12, r4, r5, r6, r7, r8, r9, r10, r11;
    char        function[16];
    uint32_t    line_number;
    uint32_t    tick_count;
    uint32_t    loop_count;
    uint32_t    free_heap_min;
} CrashRecord;

#define CRASH_RAM_BASE   ((volatile CrashRecord*)0x20007FC0)
static CrashRecord* g_crash = nullptr;

void initCrashDetection() {
    g_crash = (CrashRecord*)CRASH_RAM_BASE;
    if (!g_crash->magic) {
        memset((void*)g_crash, 0, sizeof(CrashRecord));
        g_crash->magic = CRASH_MAGIC;
    }

    // Enable fault detection
    SCB->CFSR = 0xFFFFFFFF;
    SCB->HFSR = 0xFFFFFFFF;
    SCB->BFAR = 0;
    SCB->AFSR = 0;

    // Detect div-by-zero, unaligned access
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;
    SCB->CCR |= SCB_CCR_UNALIGN_TRP_Msk;

    // Enable MemManage, BusFault, UsageFault
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk;
}

void recordCrash(uint32_t lr, uint32_t cfsr_val, uint32_t hfsr_val,
                 const char* func_name, uint32_t line_num) {
    if (!g_crash) g_crash = (CrashRecord*)CRASH_RAM_BASE;

    uint32_t msp, psp_main, psp_isr;
    __asm volatile("mrs %0, msp" : "=r"(msp));

    uint32_t ccr = SCB->CCR;
    bool privileged = (ccr & 0x4) == 0;
    if (privileged) {
        __asm volatile("mrs %0, psp" : "=r"(psp_main));
        psp_isr = 0;
    } else {
        psp_main = 0;
        __asm volatile("mrs %0, psp" : "=r"(psp_isr));
    }

    uint32_t xPSR;
    __asm volatile("mrs %0, xpsr" : "=r"(xPSR));

    // Get exception frame registers from stack
    uint32_t regs[14];
    __asm volatile(
        "mrs r0, psp \n"
        "ldr r1, [r0, #0]  \n"
        "ldr r2, [r0, #4]  \n"
        "ldr r3, [r0, #8]  \n"
        "ldr r4, [r0, #12] \n"
        "ldr r5, [r0, #16] \n"
        "ldr r6, [r0, #20] \n"
        "ldr r7, [r0, #24] \n"
        "ldr r8, [r0, #28] \n"
        "ldr r9, [r0, #32] \n"
        "ldr r10, [r0, #36] \n"
        "ldr r11, [r0, #40] \n"
        "ldr r12, [r0, #44] \n"
        : "=r"(regs[0]), "=r"(regs[1]), "=r"(regs[2]), "=r"(regs[3]),
          "=r"(regs[4]), "=r"(regs[5]), "=r"(regs[6]), "=r"(regs[7]),
          "=r"(regs[8]), "=r"(regs[9]), "=r"(regs[10]), "=r"(regs[11]),
          "=r"(regs[12])
    );

    g_crash->msp_before_crash = msp;
    g_crash->psp_main = psp_main;
    g_crash->psp_isr = psp_isr;
    g_crash->lr_at_crash = lr;
    g_crash->cfsr = cfsr_val;
    g_crash->hfsr = hfsr_val;
    g_crash->bfar = SCB->BFAR;
    g_crash->afsr = SCB->AFSR;
    g_crash->xPSR = xPSR;
    g_crash->r0 = regs[0];  g_crash->r1 = regs[1]; g_crash->r2 = regs[2]; g_crash->r3 = regs[3];
    g_crash->r12 = regs[4]; g_crash->r4 = regs[5]; g_crash->r5 = regs[6]; g_crash->r6 = regs[7];
    g_crash->r7 = regs[8];  g_crash->r8 = regs[9]; g_crash->r9 = regs[10]; g_crash->r10 = regs[11];
    g_crash->r11 = regs[12];

    uint8_t len = 0;
    if (func_name) {
        while (len < 15 && func_name[len]) len++;
        memcpy((void*)g_crash->function, func_name, len);
        g_crash->function[len] = '\0';
    } else {
        g_crash->function[0] = '?';
        g_crash->function[1] = '\0';
    }
    g_crash->line_number = line_num;
    g_crash->tick_count = millis();

    extern size_t getFreeHeap();
    extern uint32_t g_free_heap_min;
    if (!g_crash->free_heap_min || (getFreeHeap() < g_crash->free_heap_min)) {
        g_crash->free_heap_min = getFreeHeap();
    }
}

void dumpCrashInfo(bool fatal) {
    if (!g_crash || !g_crash->magic) return;

    uint32_t cfsr = g_crash->cfsr;

// crashCount is declared in main.ino as volatile uint32_t crashCount = 0x5A5A5A5A;
    extern volatile uint32_t crashCount;
    
    SerialMon.println("\n\n################################");
    SerialMon.print(F("### CRASH DETECTED (reboot #"));
    SerialMon.print(g_crash->tick_count > 0 ? ((crashCount + 1) & 0xFF) : 0);
    SerialMon.println(") ###");
    SerialMon.println("################################");

    SerialMon.print(F("  LR=0x"));   SerialMon.println(g_crash->lr_at_crash, HEX);
    SerialMon.print(F("  MSP=0x"));   SerialMon.println(g_crash->msp_before_crash, HEX);
    SerialMon.print(F("  PSP_main=0x"));  SerialMon.println(g_crash->psp_main, HEX);
    SerialMon.print(F("  PSP_isr=0x"));   SerialMon.println(g_crash->psp_isr, HEX);
    SerialMon.print(F("  xPSR=0x"));      SerialMon.println(g_crash->xPSR, HEX);

    SerialMon.print(F("  CFSR: ")); SerialMon.println(cfsr, HEX);
    if (cfsr & SCB_CFSR_IACCVIOL_Msk) SerialMon.println("    -> Illegal instruction access");
    if (cfsr & SCB_CFSR_UNDEFINSTR_Msk) SerialMon.println("    -> Undefined instruction");
    if (cfsr & SCB_CFSR_DIVBYZERO_Msk) {
        SerialMon.println("    -> Divide-by-zero!");
        SerialMon.print(F("    r0=0x")); SerialMon.print(g_crash->r0, HEX);
        SerialMon.print(F(" r1=0x")); SerialMon.print(g_crash->r1, HEX);
        SerialMon.println();
    }
    if (cfsr & SCB_CFSR_UNALIGNED_Msk) SerialMon.println("    -> Unaligned access fault");

    if (cfsr & SCB_CFSR_MMARVALID_Msk) {
        SerialMon.print(F("  BFAR=0x")); SerialMon.println(g_crash->bfar, HEX);
        SerialMon.print(F("    Faulting data address: 0x"));
        SerialMon.println(g_crash->bfar, HEX);

        if (g_crash->bfar >= 0x20000000 && g_crash->bfar < 0x30000000) {
            SerialMon.println("    -> RAM access fault");
        } else if (g_crash->bfar >= 0x40000000) {
            SerialMon.println("    -> Peripheral bus fault");
        } else if (g_crash->bfar == 0) {
            SerialMon.println("    -> Null pointer dereference!");
        } else {
            SerialMon.println("    -> Unmapped memory access");
        }
    }

    uint32_t hfsr = g_crash->hfsr;
    if (hfsr & SCB_HFSR_VECTTBL_Msk) {
        SerialMon.println("  HFSR: Vector table read fault");
    }
    if (hfsr & SCB_HFSR_FORCED_Msk) {
        SerialMon.println("  HFSR: Forced HardFault (combined fault)");
    }

    SerialMon.print(F("  Function: "));
    SerialMon.print(g_crash->function);
    SerialMon.print(F(":line "));
    SerialMon.println(g_crash->line_number);
    SerialMon.print(F("  millis at crash: "));
    SerialMon.println(g_crash->tick_count);

    SerialMon.println("\n  Registers:");
    char regbuf[64];
    snprintf(regbuf, sizeof(regbuf), "    r0=0x%08X r1=0x%08X r2=0x%08X r3=0x%08X",
             g_crash->r0, g_crash->r1, g_crash->r2, g_crash->r3);
    SerialMon.println(regbuf);
    snprintf(regbuf, sizeof(regbuf), "    r4=0x%08X r5=0x%08X r6=0x%08X r7=0x%08X",
             g_crash->r4, g_crash->r5, g_crash->r6, g_crash->r7);
    SerialMon.println(regbuf);
    snprintf(regbuf, sizeof(regbuf), "    r8=0x%08X r9=0x%08X r10=0x%08X r11=0x%08X",
             g_crash->r8, g_crash->r9, g_crash->r10, g_crash->r11);
    SerialMon.println(regbuf);
    snprintf(regbuf, sizeof(regbuf), "    r12=0x%08X lr=0x%08X",
             g_crash->r12, g_crash->lr_at_crash);
    SerialMon.println(regbuf);

    extern uint32_t g_stack_bottom;
    if (g_crash->stack_guard_pattern == STACK_GUARD_PAT) {
        SerialMon.println("  [OK] Stack guard intact");
    } else {
        uint32_t current_val = *(volatile uint32_t*)0x20007FFC;
        if (current_val != STACK_GUARD_PAT) {
            uint32_t consumed = (STACK_GUARD_PAT - current_val) / 4;
            SerialMon.print("  [CRITICAL] Stack overflow! ");
            SerialMon.print(consumed);
            SerialMon.println(" words consumed");
        } else {
            SerialMon.println("  [OK] Stack guard intact (at top of RAM)");
        }
    }

    if (g_crash->free_heap_min) {
        SerialMon.print(F("  Min free heap: "));
        SerialMon.println(g_crash->free_heap_min);
    }

    uint32_t lr = g_crash->lr_at_crash;
    if ((lr & 0x1000000) == 0) {
        SerialMon.print(F("[DIAG] LR=0x"));
        SerialMon.println(lr, HEX);
        SerialMon.println("    -> Function return (not EXC_RETURN)");
    }

    // Inline clear since we're in same translation unit
    if (g_crash && g_crash->magic) {
        g_crash->magic = 0;
        memset((void*)g_crash, 0, sizeof(CrashRecord));
    }
}

void clearCrashInfo();  // Forward declaration — defined after dumpCrashInfo

#define STACK_GUARD_ADDR  ((volatile uint32_t*)0x20007FFC)

void initStackGuard() {
    *STACK_GUARD_ADDR = STACK_GUARD_PAT;
}

bool checkStackOverflow() {
    if (*STACK_GUARD_ADDR != STACK_GUARD_PAT) {
        uint32_t consumed = (STACK_GUARD_PAT - *STACK_GUARD_ADDR) / 4;
        SerialMon.print(F("### Stack overflow! "));
        SerialMon.print(consumed);
        SerialMon.println(" words consumed ###\n");
        return true;
    }
    return false;
}

// crashCount is declared in main.ino as volatile uint32_t crashCount = 0x5A5A5A5A;

void reportCrashState() {
    if (!g_crash || !g_crash->magic) return;

    dumpCrashInfo(true);

    uint32_t cfsr = g_crash->cfsr;

    if (cfsr & SCB_CFSR_DIVBYZERO_Msk) {
        SerialMon.println("\n[DIAG] ** Likely cause: Divide-by-zero");
        SerialMon.print(F("  In function ")); SerialMon.print(g_crash->function);
        SerialMon.print(F(" at line " )); SerialMon.println(g_crash->line_number);
    }

    if (cfsr & SCB_CFSR_UNALIGNED_Msk) {
        SerialMon.println("\n[DIAG] ** Likely cause: Unaligned memory access");
        if (g_crash->bfar % 4 != 0) {
            SerialMon.print(F("  Address ")); SerialMon.print(g_crash->bfar, HEX);
            SerialMon.println(" is not word-aligned");
        }
    }

    if (g_crash->bfar == 0 && cfsr & SCB_CFSR_MMARVALID_Msk) {
        SerialMon.println("\n[DIAG] ** Likely cause: Null pointer dereference");
        SerialMon.print(F("  In function ")); SerialMon.print(g_crash->function);
        SerialMon.print(F(" at line " )); SerialMon.println(g_crash->line_number);
    }

    if (g_crash->bfar >= 0x40000000 && g_crash->bfar < 0x50000000) {
        SerialMon.println("\n[DIAG] ** Likely cause: Peripheral bus access fault");
        SerialMon.print(F("  Register address ")); SerialMon.println(g_crash->bfar, HEX);

        if (g_crash->bfar >= 0x40002000 && g_crash->bfar < 0x40003000) {
            SerialMon.println("    -> NRF_SPIM2 region (display SPI)");
        } else if (g_crash->bfar >= 0x40003000 && g_crash->bfar < 0x40004000) {
            SerialMon.println("    -> NRF_SPIM3 region (LoRa SPI)");
        } else if (g_crash->bfar >= 0x40004000 && g_crash->bfar < 0x40005000) {
            SerialMon.println("    -> NRF_TWIM1 region (I2C/SWD/RTC)");
        } else if (g_crash->bfar >= 0x4000E000 && g_crash->bfar < 0x4000F000) {
            SerialMon.println("    -> NRF_UARTE2 region (UART)");
        }
    }

    extern volatile uint32_t crashCount;
    if (crashCount > 3) {
        SerialMon.print(F("\n[DIAG] WARNING: "));
        SerialMon.print(crashCount);
        SerialMon.println(" consecutive crashes — possible infinite reset loop");
    }

    clearCrashInfo();
}

// Debug logging buffer
#define DEBUG_LOG_MAX_ENTRIES 32

typedef struct {
    uint32_t tick;
    char text[40];
} DebugEntry;

#define DBG_BUF_ADDR ((volatile DebugEntry*)0x20006FC0)
static DebugEntry* g_dbg_buf = nullptr;

void initDebugBuffer() {
    g_dbg_buf = (DebugEntry*)DBG_BUF_ADDR;
    for (int i = 0; i < DEBUG_LOG_MAX_ENTRIES; i++) {
        g_dbg_buf[i].tick = 0;
        g_dbg_buf[i].text[0] = '\0';
    }
}

void dbgLog(const char* fmt, ...) {
    if (!g_dbg_buf) initDebugBuffer();
    if (!g_dbg_buf) return;

    static uint8_t idx = 0;

    char buf[41];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    g_dbg_buf[idx].tick = millis();
    strncpy(g_dbg_buf[idx].text, buf, 39);
    g_dbg_buf[idx].text[40] = '\0';

    idx = (idx + 1) % DEBUG_LOG_MAX_ENTRIES;
}

void dumpDebugBuffer() {
    if (!g_dbg_buf) return;
    SerialMon.println("\n### Debug Buffer Log ###");
    for (int i = 0; i < DEBUG_LOG_MAX_ENTRIES; i++) {
        if (g_dbg_buf[i].tick) {
            char line[56];
            snprintf(line, sizeof(line), "[%lu] %s", g_dbg_buf[i].tick, g_dbg_buf[i].text);
            SerialMon.println(line);
        }
    }
}

uint32_t g_free_heap_min = UINT32_MAX;
void trackHeapUsage() {
    extern size_t getFreeHeap();
    size_t free_now = getFreeHeap();
    if (free_now < g_free_heap_min) {
        g_free_heap_min = free_now;
    }
}

size_t getMinFreeHeap() {
    return g_free_heap_min;
}

void clearCrashInfo() {
    if (!g_crash || !g_crash->magic) return;
    g_crash->magic = 0;
    memset((void*)g_crash, 0, sizeof(CrashRecord));
}

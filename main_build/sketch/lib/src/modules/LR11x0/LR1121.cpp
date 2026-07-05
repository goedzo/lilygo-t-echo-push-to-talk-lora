#line 1 "V:\\Bmad\\Project_ptt_lora\\main\\lib\\src\\modules\\LR11x0\\LR1121.cpp"
#include "LR1121.h"
#if !RADIOLIB_EXCLUDE_LR11X0

LR1121::LR1121(Module* mod) : LR1120(mod) {
  chipType = RADIOLIB_LR11X0_DEVICE_LR1121;
}

#endif
/*
 * ⚠️  DEPRECATED: board_init() has been moved to WeightScale_Base.ino
 * 
 * REASON: Arduino IDE doesn't properly configure include paths for nested
 * library dependencies. ESP32_Display_Panel depends on esp-lib-utils, which
 * created compilation errors when board_init.cpp was compiled separately.
 * 
 * SOLUTION: Moved board_init() implementation into WeightScale_Base.ino
 * where all includes are guaranteed to be in scope.
 * 
 * This file is intentionally empty. You can safely delete it.
 */

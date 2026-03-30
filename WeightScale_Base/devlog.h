#pragma once
#include <Arduino.h>

void devlog_init(void);
void devlog_printf(const char *fmt, ...);
String devlog_get_text(void);
void devlog_clear(void);
void devlog_load_from_storage(void);
void devlog_save_to_storage(void);

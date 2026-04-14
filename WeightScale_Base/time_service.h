#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <time.h>

void time_service_init(void);
uint32_t time_service_now(void);
uint32_t time_service_today_epoch_day(void);
bool time_service_is_valid(void);
void time_service_request_ntp_sync(void);
bool time_service_set_datetime(int year, int month, int day,
                               int hour, int minute, int second);
bool time_service_get_local_tm(struct tm *out);
void time_service_format_hhmm(char *out, size_t max);
void time_service_format_datetime(char *out, size_t max);

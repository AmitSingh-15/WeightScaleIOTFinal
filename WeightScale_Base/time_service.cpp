#include "time_service.h"
#include <Arduino.h>
#include <time.h>
#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>

static bool g_tz_initialized = false;
static bool g_ntp_started = false;

static void time_service_ensure_timezone(void)
{
    if(g_tz_initialized) return;
    setenv("TZ", "IST-5:30", 1);
    tzset();
    g_tz_initialized = true;
}

void time_service_init(void)
{
    time_service_ensure_timezone();
}

void time_service_request_ntp_sync(void)
{
    time_service_ensure_timezone();

    if(!g_ntp_started) {
        configTime(19800, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
        g_ntp_started = true;
    } else {
        configTime(19800, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
    }
}

uint32_t time_service_now(void)
{
    time_service_ensure_timezone();
    return (uint32_t)time(nullptr);
}

uint32_t time_service_today_epoch_day(void)
{
    time_service_ensure_timezone();
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);

    t.tm_hour = 0;
    t.tm_min  = 0;
    t.tm_sec  = 0;

    return (uint32_t)mktime(&t);
}

bool time_service_is_valid(void)
{
    return time_service_now() > 86400U;
}

bool time_service_set_datetime(int year, int month, int day,
                               int hour, int minute, int second)
{
    time_service_ensure_timezone();

    if(year < 2024 || year > 2099) return false;
    if(month < 1 || month > 12) return false;
    if(day < 1 || day > 31) return false;
    if(hour < 0 || hour > 23) return false;
    if(minute < 0 || minute > 59) return false;
    if(second < 0 || second > 59) return false;

    struct tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon  = month - 1;
    t.tm_mday = day;
    t.tm_hour = hour;
    t.tm_min  = minute;
    t.tm_sec  = second;
    t.tm_isdst = -1;

    time_t epoch = mktime(&t);
    if(epoch <= 0) return false;

    struct timeval tv = {};
    tv.tv_sec = epoch;
    tv.tv_usec = 0;
    return settimeofday(&tv, NULL) == 0;
}

bool time_service_get_local_tm(struct tm *out)
{
    if(!out) return false;
    time_service_ensure_timezone();

    time_t now = time(nullptr);
    if(now <= 0) return false;

    localtime_r(&now, out);
    return true;
}

void time_service_format_hhmm(char *out, size_t max)
{
    if(!out || max == 0) return;

    struct tm t;
    if(!time_service_get_local_tm(&t) || !time_service_is_valid()) {
        snprintf(out, max, "--:--");
        return;
    }

    strftime(out, max, "%H:%M", &t);
}

void time_service_format_datetime(char *out, size_t max)
{
    if(!out || max == 0) return;

    struct tm t;
    if(!time_service_get_local_tm(&t) || !time_service_is_valid()) {
        snprintf(out, max, "Time not set");
        return;
    }

    strftime(out, max, "%Y-%m-%d %H:%M", &t);
}

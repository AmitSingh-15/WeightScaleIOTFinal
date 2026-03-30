#include "time_service.h"
#include <time.h>


uint32_t time_service_now(void)
{
    return (uint32_t)time(nullptr);
}

uint32_t time_service_today_epoch_day(void)
{
    time_t now = time(nullptr);
    struct tm t;
    localtime_r(&now, &t);

    t.tm_hour = 0;
    t.tm_min  = 0;
    t.tm_sec  = 0;

    return (uint32_t)mktime(&t);
}

#include "devlog.h"
#include <cstdarg>

/* In-memory ring buffer for developer log display */
#define DEVLOG_BUF_SIZE 2048
static char s_log_buf[DEVLOG_BUF_SIZE];
static size_t s_log_len = 0;

void devlog_init(void) 
{
    s_log_buf[0] = '\0';
    s_log_len = 0;
    Serial.println("[DEVLOG] Initialized");
}

void devlog_printf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    
    char line[160];
    int n = vsnprintf(line, sizeof(line), fmt, args);
    va_end(args);
    if (n < 0) return;

    Serial.println(line);

    /* Append to ring buffer with newline */
    size_t line_len = strlen(line);
    size_t needed = line_len + 1; /* +1 for '\n' */

    /* If buffer would overflow, discard oldest half */
    if (s_log_len + needed >= DEVLOG_BUF_SIZE - 1) {
        size_t half = s_log_len / 2;
        /* Find the next newline after the halfway point */
        const char *cut = strchr(s_log_buf + half, '\n');
        if (cut) {
            cut++; /* skip past newline */
            size_t keep = s_log_len - (cut - s_log_buf);
            memmove(s_log_buf, cut, keep);
            s_log_len = keep;
        } else {
            s_log_buf[0] = '\0';
            s_log_len = 0;
        }
    }

    memcpy(s_log_buf + s_log_len, line, line_len);
    s_log_len += line_len;
    s_log_buf[s_log_len++] = '\n';
    s_log_buf[s_log_len] = '\0';
}

String devlog_get_text(void) 
{
    if (s_log_len == 0) return String("(no logs)");
    return String(s_log_buf);
}

void devlog_clear(void) 
{
    s_log_buf[0] = '\0';
    s_log_len = 0;
}

void devlog_load_from_storage(void) 
{
}

void devlog_save_to_storage(void) 
{
}

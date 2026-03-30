#include "devlog.h"
#include "storage_service.h"
#include <stdarg.h>

static const int MAX_LINES = 200;
static const int MAX_LINE_LEN = 128;
static char lines[MAX_LINES][MAX_LINE_LEN];
static int head = 0;
static int count_lines = 0;

void devlog_init(void)
{
    head = 0;
    count_lines = 0;
    for(int i=0;i<MAX_LINES;i++) lines[i][0]=0;
}

void devlog_load_from_storage(void)
{
    String stored = storage_load_devlog();
    if(stored.length() == 0) return;

    /* Parse stored log (newline-separated lines) */
    int idx = 0;
    int line_idx = 0;
    int col = 0;

    for(int i = 0; i < stored.length() && line_idx < MAX_LINES; i++)
    {
        char c = stored[i];
        if(c == '\n')
        {
            if(col > 0) line_idx++;
            col = 0;
        }
        else
        {
            if(col < MAX_LINE_LEN - 1)
            {
                lines[line_idx][col++] = c;
            }
        }
    }

    if(col > 0) line_idx++;
    
    count_lines = line_idx;
    head = line_idx % MAX_LINES;
}

void devlog_printf(const char *fmt, ...)
{
    char buf[MAX_LINE_LEN];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    // append to circular buffer
    strncpy(lines[head], buf, MAX_LINE_LEN-1);
    lines[head][MAX_LINE_LEN-1] = 0;
    head = (head + 1) % MAX_LINES;
    if(count_lines < MAX_LINES) count_lines++;

    // also print to Serial for console
    Serial.println(buf);

    // save to persistent storage (every write)
    devlog_save_to_storage();
}

void devlog_save_to_storage(void)
{
    String s;
    int idx = (head - count_lines + MAX_LINES) % MAX_LINES;
    for(int i=0;i<count_lines;i++)
    {
        s += String(lines[idx]);
        s += "\n";
        idx = (idx + 1) % MAX_LINES;
    }
    
    if(s.length() > 0)
    {
        storage_save_devlog(s.c_str());
    }
}

String devlog_get_text(void)
{
    String s;
    int idx = (head - count_lines + MAX_LINES) % MAX_LINES;
    for(int i=0;i<count_lines;i++)
    {
        s += String(lines[idx]);
        s += "\n";
        idx = (idx + 1) % MAX_LINES;
    }
    return s;
}

void devlog_clear(void)
{
    for(int i=0;i<MAX_LINES;i++) lines[i][0]=0;
    head = 0;
    count_lines = 0;
    storage_clear_devlog();
}

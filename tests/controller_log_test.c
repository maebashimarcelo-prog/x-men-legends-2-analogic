/* Exercise the actual core logger with an owned sink; no game or hooks. */
#include "../src/controller_trace.c"
#undef NDEBUG
#include <assert.h>
static unsigned writes;
static char last_line[720];
void analog_log_event(const char *line) { writes++; snprintf(last_line,sizeof last_line,"%s",line); }
int main(void) {
    unsigned i;
    trace_enabled=0;
    for(i=0;i<10000;i++) event("GAME ENUM event=%u",i);
    assert(!writes);
    status("TRACE REFUSED owned fixture");
    assert(writes==1 && strstr(last_line,"TRACE REFUSED owned fixture"));
    trace_enabled=1; event("DIAGNOSTIC event=%d",42);
    assert(writes==2 && strstr(last_line,"DIAGNOSTIC event=42"));
    puts("PASS: 10000 diagnostic events emit nothing with release defaults; support/refusal remains visible; trace opt-in works");
    return 0;
}

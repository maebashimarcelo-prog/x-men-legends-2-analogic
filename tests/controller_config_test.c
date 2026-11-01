#include "controller_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

volatile unsigned int g_gate_calls, g_gate_movement, g_gate_selected_pad, g_gate_radial;
void analog_log_event(const char *message) { (void)message; }
int analog_identify_build(XtaBuild *build) {
    memset(build,0,sizeof *build); build->status=XTA_UNKNOWN; return 1;
}
int analog_hook_install(const XtaBuild *build,const XtaConfig *config) {
    (void)build; (void)config; return 0;
}
/* Test the actual reader used by the expansion, without installing a hook. */
#include "../src/controller_analog.c"

int main(int argc,char **argv) {
    XtaConfig config;
    int valid,mode;
    if (argc!=2) return 1;
    mode=atoi(argv[1]);
    valid=read_config(GetModuleHandleW(NULL),&config);
    if (mode==2) {
        if (valid) return 1;
        puts("PASS: invalid Player1 override refuses analog configuration."); return 0;
    }
    if (!valid || config.preserve_magnitude!=1) return 1;
    if (mode==0 && (config.enabled!=1 || config.controller_index!=0 ||
        config.deadzone<0.149f || config.deadzone>0.151f || config.debug!=0)) return 1;
    if (mode==1 && (config.enabled!=0 || config.controller_index!=3 ||
        config.deadzone!=0.25f || config.debug!=1)) return 1;
    puts(mode==0?"PASS: unchanged legacy INI remains compatible.":
                 "PASS: Player1 overrides and General diagnostic settings take precedence.");
    return 0;
}

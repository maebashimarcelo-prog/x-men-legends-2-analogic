#ifndef XML2_TRUE_ANALOG_H
#define XML2_TRUE_ANALOG_H

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdint.h>

#ifdef XML2_MODERN_CONTROLLER
#include "project_version.h"
#define XTA_VERSION XMC_VERSION
#define XTA_PRODUCT "XML2 Modern Controller"
#else
#define XTA_VERSION "0.1.0"
#define XTA_PRODUCT "XML2 True Analog"
#endif
#define XTA_KNOWN_SHA256 "146CD9C316EDB57A267CD73753A7CE9AF647E52AAB750D449C6B278FB4A1669B"

typedef enum {
    XTA_SUPPORTED,
    XTA_UNKNOWN,
    XTA_INCOMPATIBLE
} XtaBuildStatus;

typedef struct {
    XtaBuildStatus status;
    char sha256[65];
    char reason[96];
    uint64_t file_size;
    uint32_t pe_timestamp;
    uint32_t preferred_base;
    uint32_t image_size;
    unsigned pattern_matches;
    unsigned char *gate;
    uintptr_t threshold_address;
    uintptr_t wrapper_cell;
    uintptr_t player_slots;
    float retail_threshold;
} XtaBuild;

typedef struct {
    int enabled;
    float deadzone;
    int preserve_magnitude;
    unsigned controller_index;
    int debug;
} XtaConfig;

void analog_log_event(const char *message);
void analog_init(HMODULE self_module);
int analog_identify_build(XtaBuild *build);
int analog_hook_install(const XtaBuild *build, const XtaConfig *config);
int analog_direction_install(const XtaBuild *build, const XtaConfig *config);
void analog_probe_start(const XtaBuild *build, const XtaConfig *config);

extern volatile unsigned int g_gate_calls;
extern volatile unsigned int g_gate_movement;
extern volatile unsigned int g_gate_selected_pad;
extern volatile unsigned int g_gate_radial;

#endif

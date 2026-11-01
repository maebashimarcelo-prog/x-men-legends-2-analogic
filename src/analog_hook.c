/* In-memory movement-only gate override for a validated retail profile. */
#include "xml2_analog.h"

#include <stdio.h>
#include <string.h>

void *g_gate_resume;
uintptr_t g_wrapper_cell;
unsigned int g_controller_mask;
unsigned int g_pad_offset;
unsigned int g_deadzone_sq;
float g_zero_threshold;
float g_retail_threshold;
volatile unsigned int g_gate_calls;
volatile unsigned int g_gate_movement;
volatile unsigned int g_gate_selected_pad;
volatile unsigned int g_gate_radial;
extern void analog_gate_stub(void);

int analog_hook_install(const XtaBuild *build, const XtaConfig *config) {
    unsigned char *gate = build->gate;
    unsigned char original[6];
    unsigned char patch[6] = {0xE9,0,0,0,0,0x90};
    static const unsigned char continuation[7] = {0xDF,0xE0,0xF6,0xC4,0x41,0x75,0x3D};
    DWORD old_protect, restored;
    uintptr_t threshold_address;
    int threshold;
    int32_t displacement;
    char message[160];

    if (build->status != XTA_SUPPORTED || !gate || config->controller_index > 9 ||
        config->deadzone < 0.0f || config->deadzone > 0.5f) {
        analog_log_event("Hook: NOT installed; invalid build or configuration\r\n");
        return 0;
    }
    memcpy(&threshold_address, gate + 2, sizeof threshold_address);
    if (gate[0] != 0xD8 || gate[1] != 0x1D ||
        threshold_address != build->threshold_address ||
        memcmp(gate + 6, continuation, sizeof continuation)) {
        analog_log_event("Hook: NOT installed; movement instructions changed\r\n");
        return 0;
    }
    memcpy(original, gate, sizeof original);
    threshold = (int)(config->deadzone * 1000.0f + 0.5f);
    g_deadzone_sq = (unsigned int)(threshold * threshold);
    g_zero_threshold = 0.0f;
    g_retail_threshold = build->retail_threshold;
    g_wrapper_cell = build->wrapper_cell;
    g_controller_mask = 1u << config->controller_index;
    g_pad_offset = 0x4F0u + config->controller_index * 0x110u;
    g_gate_resume = gate + sizeof patch;
    displacement = (int32_t)((uintptr_t)&analog_gate_stub - ((uintptr_t)gate + 5));
    memcpy(patch + 1, &displacement, sizeof displacement);
    if (!VirtualProtect(gate, sizeof patch, PAGE_EXECUTE_READWRITE, &old_protect)) {
        analog_log_event("Hook: NOT installed; VirtualProtect failed\r\n");
        return 0;
    }
    memcpy(gate, patch, sizeof patch);
    if (!FlushInstructionCache(GetCurrentProcess(), gate, sizeof patch)) {
        memcpy(gate, original, sizeof original);
        FlushInstructionCache(GetCurrentProcess(), gate, sizeof original);
        VirtualProtect(gate, sizeof patch, old_protect, &restored);
        analog_log_event("Hook: NOT installed; instruction-cache flush failed\r\n");
        return 0;
    }
    if (!VirtualProtect(gate, sizeof patch, old_protect, &restored)) {
        analog_log_event("Hook: installed, but page protection restore failed\r\n");
    }
    snprintf(message, sizeof message,
             "Hook: Installed at %p; slots 0/1, controller %u, radial deadzone %.3f\r\n",
             (void *)gate, config->controller_index, config->deadzone);
    analog_log_event(message);
    return 1;
}

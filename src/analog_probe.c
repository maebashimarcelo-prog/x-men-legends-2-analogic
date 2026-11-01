/* Configuration, startup diagnostics and optional read-only input sampling. */
#include "xml2_analog.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

static LONG started;
static XtaBuild observed_build;
static XtaConfig observed_config;

static int readable(const void *pointer, SIZE_T bytes) {
    MEMORY_BASIC_INFORMATION region;
    uintptr_t address = (uintptr_t)pointer;
    if (!pointer || VirtualQuery(pointer, &region, sizeof region) != sizeof region) return 0;
    if (region.State != MEM_COMMIT || (region.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
    return address >= (uintptr_t)region.BaseAddress &&
           bytes <= (uintptr_t)region.BaseAddress + region.RegionSize - address;
}

static int config_path(HMODULE module, WCHAR path[MAX_PATH]) {
    WCHAR *name;
    if (!GetModuleFileNameW(module, path, MAX_PATH)) return 0;
    name = wcsrchr(path, L'\\');
    if (!name || (size_t)(name - path) + 16 >= MAX_PATH) return 0;
    lstrcpyW(name + 1, L"true-analog.ini");
    return GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES;
}

static int read_config(HMODULE module, XtaConfig *config) {
    WCHAR path[MAX_PATH], deadzone_text[32], *end;
    int index;
    memset(config, 0, sizeof *config);
    if (!config_path(module, path)) {
        analog_log_event("Config: true-analog.ini missing\r\n");
        return 0;
    }
    config->enabled = GetPrivateProfileIntW(L"Controller", L"TrueAnalogMovement", -1, path);
    config->preserve_magnitude = GetPrivateProfileIntW(L"Controller", L"PreserveMagnitude", -1, path);
    config->debug = GetPrivateProfileIntW(L"Controller", L"DebugInput", -1, path);
    index = GetPrivateProfileIntW(L"Controller", L"ControllerIndex", -1, path);
    GetPrivateProfileStringW(L"Controller", L"Deadzone", L"", deadzone_text, 32, path);
    config->deadzone = wcstof(deadzone_text, &end);
    if (config->enabled < 0 || config->enabled > 1 ||
        config->preserve_magnitude < 0 || config->preserve_magnitude > 1 ||
        config->debug < 0 || config->debug > 1 || index < 0 || index > 9 ||
        end == deadzone_text || *end || !isfinite(config->deadzone) ||
        config->deadzone < 0.0f || config->deadzone > 0.5f) {
        analog_log_event("Config: invalid value; hook disabled\r\n");
        return 0;
    }
    config->controller_index = (unsigned)index;
    return 1;
}

static DWORD WINAPI sample_thread(void *unused) {
    DWORD last_log = 0, last_counter_log = 0, last_state_log = 0;
    int last_x = 0, last_y = 0;
    const unsigned index = observed_config.controller_index;
    const uintptr_t cell = observed_build.wrapper_cell;
    const uintptr_t slots = observed_build.player_slots;
    (void)unused;
    for (;;) {
        uintptr_t wrapper;
        uint32_t mask;
        int32_t x, y;
        float sx, sy, radius, angle, slot_x, slot_y;
        DWORD now;
        char message[256];
        Sleep(16);
        if (!readable((void *)cell, sizeof wrapper) || !readable((void *)slots, 8)) continue;
        wrapper = *(const uintptr_t *)cell;
        if (!wrapper || wrapper > UINTPTR_MAX - 0x129CCu ||
            !readable((void *)(wrapper + 0x129CCu), sizeof mask)) continue;
        mask = *(const uint32_t *)(wrapper + 0x129CCu);
        now = GetTickCount();
        if (g_gate_calls && now - last_counter_log >= 1000) {
            snprintf(message, sizeof message,
                     "Gate: calls=%u movement=%u selected_pad=%u radial=%u mask=0x%X\r\n",
                     g_gate_calls, g_gate_movement, g_gate_selected_pad, g_gate_radial,
                     (unsigned)(mask & 0x3FFu));
            analog_log_event(message);
            last_counter_log = now;
        }
        /* Report state even when the selected controller is not flagged active.
           This is strictly read-only and helps diagnose the pad routing. */
        if (now - last_state_log >= 1000) {
            float ax = *(const float *)slots;
            float ay = *(const float *)(slots + 4);
            if (isfinite(ax) && isfinite(ay)) {
                snprintf(message, sizeof message,
                         "State: t=%lu selected=%u active=%u mask=0x%03X slot01=(%.4f,%.4f)\r\n",
                         (unsigned long)now, index,
                         (unsigned)((mask & (1u << index)) != 0),
                         (unsigned)(mask & 0x3FFu), ax, ay);
                analog_log_event(message);
            }
            last_state_log = now;
        }
        if (!(mask & (1u << index))) continue;
        if (wrapper > UINTPTR_MAX - (0x4F0u + index * 0x110u + 8u) ||
            !readable((void *)(wrapper + 0x4F0u + index * 0x110u), 8)) continue;
        x = *(const int32_t *)(wrapper + 0x4F0u + index * 0x110u);
        y = *(const int32_t *)(wrapper + 0x4F4u + index * 0x110u);
        slot_x = *(const float *)slots;
        slot_y = *(const float *)(slots + 4);
        if (x < -1000 || x > 1000 || y < -1000 || y > 1000 ||
            !isfinite(slot_x) || !isfinite(slot_y)) continue;
        if (x == last_x && y == last_y && now - last_log < 1000) continue;
        if (now - last_log < 100) continue;
        sx = x / 1000.0f;
        sy = y / 1000.0f;
        radius = hypotf(sx, sy);
        angle = atan2f(-sy, sx) * 57.2957795f;
        if (angle < 0) angle += 360.0f;
        snprintf(message, sizeof message,
                 "Input: t=%lu pad=%u raw=(%ld,%ld) norm=(%.3f,%.3f) mag=%.3f angle=%.1f slot01=(%.3f,%.3f)\r\n",
                 (unsigned long)now, index, (long)x, (long)y,
                 sx, sy, radius, angle, slot_x, slot_y);
        analog_log_event(message);
        last_log = now;
        last_x = x;
        last_y = y;
    }
}

void analog_probe_start(const XtaBuild *build, const XtaConfig *config) {
    HANDLE thread;
    /* Unknown builds can only be sampled when compat.c has identified the
       exact diagnostic candidate and supplied both read-only addresses. */
    if (!config->debug ||
        (build->status != XTA_SUPPORTED &&
         !(build->status == XTA_UNKNOWN && build->wrapper_cell && build->player_slots))) return;
    if (build->status == XTA_UNKNOWN)
        analog_log_event("Input: read-only diagnostic candidate; movement hook remains disabled\r\n");
    observed_build = *build;
    observed_config = *config;
    thread = CreateThread(NULL, 0, sample_thread, NULL, 0, NULL);
    if (thread) CloseHandle(thread);
    else analog_log_event("Input: diagnostic thread could not start\r\n");
}

void analog_init(HMODULE self_module) {
    XtaBuild build;
    XtaConfig config;
    char message[256];
    int valid_config;
    if (InterlockedCompareExchange(&started, 1, 0)) return;
    analog_log_event("XML2 ANALOG 360 EXPERIMENTAL V2 - BUILD ID: XML2-360-V2-20261101\r\n");
    analog_log_event(XTA_PRODUCT " v" XTA_VERSION "\r\n");
    analog_identify_build(&build);
    snprintf(message, sizeof message, "XMen2.exe SHA-256: %s\r\n",
             build.sha256[0] ? build.sha256 : "unavailable");
    analog_log_event(message);
    snprintf(message, sizeof message,
             "Build: %s; size=%llu; PE timestamp=0x%08X; image base=0x%08X\r\n",
             build.status == XTA_SUPPORTED ? "Supported" :
             build.status == XTA_UNKNOWN ? "Unknown" : "Incompatible",
             (unsigned long long)build.file_size, build.pe_timestamp,
             build.preferred_base);
    analog_log_event(message);
    snprintf(message, sizeof message, "Pattern: %s (%u match); %s\r\n",
             build.pattern_matches == 1 ? "Found" :
             build.pattern_matches ? "Ambiguous" : "Not found",
             build.pattern_matches, build.reason);
    analog_log_event(message);
    valid_config = read_config(self_module, &config);
    if (!valid_config) {
        analog_log_event("Hook: NOT installed\r\n");
        return;
    }
    snprintf(message, sizeof message,
             "True Analog: %s; Deadzone: %.3f; Preserve Magnitude: %s; ControllerIndex: %u; DebugInput: %d\r\n",
             config.enabled ? "Enabled" : "Disabled", config.deadzone,
             config.preserve_magnitude ? "Yes" : "No", config.controller_index,
             config.debug);
    analog_log_event(message);
    if (!config.enabled) analog_log_event("Hook: NOT installed (disabled)\r\n");
    else if (build.status != XTA_SUPPORTED)
        analog_log_event("Hook: NOT installed (build is not supported)\r\n");
    else if (!config.preserve_magnitude)
        analog_log_event("Hook: NOT installed (PreserveMagnitude=0 is unsupported)\r\n");
    else analog_direction_install(&build, &config);
    analog_probe_start(&build, &config);
}

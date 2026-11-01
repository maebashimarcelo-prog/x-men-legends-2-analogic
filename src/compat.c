/* Identify the retail build and locate its movement gate in executable code. */
#include "xml2_analog.h"

#include <wincrypt.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define XTA_KNOWN_SIZE 3129344u
#define XTA_KNOWN_TIMESTAMP 0x43203BAFu
#define XTA_KNOWN_IMAGE_SIZE 0x006744C6u
#define XTA_KNOWN_BASE 0x00400000u
#define XTA_KNOWN_GATE 0x0061A4FEu
#define XTA_KNOWN_THRESHOLD 0x00682C10u
#define XTA_KNOWN_WRAPPER 0x00A6ABFCu
#define XTA_KNOWN_PLAYER_SLOTS (0x0079EBC0u + 0x3Cu + 0x2FCu)

/* FSTP [esp]; mask the sign; FLD [esp]; FCOMP [threshold]; FNSTSW/TEST/JNE.
   Only the four relocated threshold-address bytes are wildcards. */
static const unsigned char gate_pattern[] = {
    0xD9,0x1C,0x24,0x81,0x24,0x24,0xFF,0xFF,0xFF,0x7F,
    0xD9,0x04,0x24,0xD8,0x1D,0,0,0,0,0xDF,0xE0,
    0xF6,0xC4,0x41,0x75,0x3D,0x8B,0x4C,0x24,0x10
};

static int readable(const void *pointer, SIZE_T size) {
    MEMORY_BASIC_INFORMATION region;
    uintptr_t address = (uintptr_t)pointer;
    if (!pointer || VirtualQuery(pointer, &region, sizeof region) != sizeof region) return 0;
    if (region.State != MEM_COMMIT || (region.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
    return address >= (uintptr_t)region.BaseAddress &&
           size <= (uintptr_t)region.BaseAddress + region.RegionSize - address;
}

static int hash_bytes(const unsigned char *bytes, DWORD size, char output[65]) {
    HCRYPTPROV provider = 0;
    HCRYPTHASH hash = 0;
    BYTE digest[32];
    DWORD digest_size = sizeof digest;
    static const char hex[] = "0123456789ABCDEF";
    unsigned i;
    int success = 0;
    if (!CryptAcquireContextW(&provider, NULL, NULL, PROV_RSA_AES, CRYPT_VERIFYCONTEXT)) return 0;
    if (CryptCreateHash(provider, CALG_SHA_256, 0, 0, &hash) &&
        CryptHashData(hash, bytes, size, 0) &&
        CryptGetHashParam(hash, HP_HASHVAL, digest, &digest_size, 0) &&
        digest_size == sizeof digest) {
        for (i = 0; i < sizeof digest; i++) {
            output[i * 2] = hex[digest[i] >> 4];
            output[i * 2 + 1] = hex[digest[i] & 15];
        }
        output[64] = 0;
        success = 1;
    }
    if (hash) CryptDestroyHash(hash);
    CryptReleaseContext(provider, 0);
    return success;
}

static unsigned char *read_exe(const WCHAR *path, DWORD *size) {
    HANDLE file;
    LARGE_INTEGER length;
    unsigned char *bytes = NULL;
    DWORD done = 0, read_count;
    file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                       NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return NULL;
    if (!GetFileSizeEx(file, &length) || length.QuadPart < 1024 ||
        length.QuadPart > 32 * 1024 * 1024) goto finish;
    *size = (DWORD)length.QuadPart;
    bytes = HeapAlloc(GetProcessHeap(), 0, *size);
    if (!bytes) goto finish;
    while (done < *size) {
        if (!ReadFile(file, bytes + done, *size - done, &read_count, NULL) || !read_count) {
            HeapFree(GetProcessHeap(), 0, bytes);
            bytes = NULL;
            break;
        }
        done += read_count;
    }
finish:
    CloseHandle(file);
    return bytes;
}

static int pattern_matches(const unsigned char *p) {
    unsigned i;
    for (i = 0; i < sizeof gate_pattern; i++) {
        if (i >= 15 && i < 19) continue;
        if (p[i] != gate_pattern[i]) return 0;
    }
    return 1;
}

int analog_identify_build(XtaBuild *build) {
    WCHAR path[MAX_PATH];
    WCHAR *name;
    unsigned char *bytes;
    DWORD size = 0;
    IMAGE_DOS_HEADER *dos;
    IMAGE_NT_HEADERS32 *nt;
    IMAGE_SECTION_HEADER *sections;
    size_t section_offset;
    unsigned i;
    uint32_t match_rva = 0;
    uintptr_t actual_base;
    uint32_t threshold_address;
    float threshold;
    int result = 0;

    memset(build, 0, sizeof *build);
    build->status = XTA_INCOMPATIBLE;
    strcpy(build->reason, "cannot read XMen2.exe");
    if (!GetModuleFileNameW(NULL, path, MAX_PATH) ||
        wcslen(path) >= MAX_PATH - 1) return 0;
    name = wcsrchr(path, L'\\');
    if (!name || _wcsicmp(name + 1, L"XMen2.exe")) {
        strcpy(build->reason, "process is not XMen2.exe");
        return 0;
    }
    bytes = read_exe(path, &size);
    if (!bytes) return 0;
    build->file_size = size;
    if (!hash_bytes(bytes, size, build->sha256)) {
        strcpy(build->reason, "SHA-256 unavailable");
        goto finish;
    }
    if (size < sizeof(IMAGE_DOS_HEADER)) goto invalid_pe;
    dos = (IMAGE_DOS_HEADER *)bytes;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0 ||
        (uint32_t)dos->e_lfanew > size - sizeof(IMAGE_NT_HEADERS32)) goto invalid_pe;
    nt = (IMAGE_NT_HEADERS32 *)(bytes + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386 ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR32_MAGIC ||
        nt->FileHeader.SizeOfOptionalHeader < sizeof(IMAGE_OPTIONAL_HEADER32) ||
        !nt->FileHeader.NumberOfSections || nt->FileHeader.NumberOfSections > 32) goto invalid_pe;
    section_offset = (size_t)dos->e_lfanew + 4 + sizeof(IMAGE_FILE_HEADER) +
                     nt->FileHeader.SizeOfOptionalHeader;
    if (section_offset > size ||
        nt->FileHeader.NumberOfSections > (size - section_offset) / sizeof(IMAGE_SECTION_HEADER)) goto invalid_pe;
    sections = (IMAGE_SECTION_HEADER *)(bytes + section_offset);
    build->pe_timestamp = nt->FileHeader.TimeDateStamp;
    build->preferred_base = nt->OptionalHeader.ImageBase;
    build->image_size = nt->OptionalHeader.SizeOfImage;
    for (i = 0; i < nt->FileHeader.NumberOfSections; i++) {
        IMAGE_SECTION_HEADER *section = &sections[i];
        DWORD offset = section->PointerToRawData;
        DWORD length = section->SizeOfRawData;
        DWORD j;
        if (!(section->Characteristics & IMAGE_SCN_MEM_EXECUTE)) continue;
        if (offset > size || length > size - offset || length < sizeof gate_pattern) goto invalid_pe;
        for (j = 0; j <= length - sizeof gate_pattern; j++) {
            if (!pattern_matches(bytes + offset + j)) continue;
            build->pattern_matches++;
            match_rva = section->VirtualAddress + j;
        }
    }
    if (strcmp(build->sha256, XTA_KNOWN_SHA256)) {
        /* Exact fingerprint diagnostics: never silently fall back to a generic build. */
        build->status = XTA_UNKNOWN;
        strcpy(build->reason, "hash has no validated memory-layout profile");
        /* Diagnostic candidate only: expose addresses for read-only sampling.
           NEVER mark this build supported or install the movement hook. */
        if (!strcmp(build->sha256,
                "8AE56DB5E3739E05977991380A6CF14165A4CF4D993D9A968369E332AFD7638C") &&
            build->file_size == 6770688u &&
            build->pe_timestamp == 0x00564544u &&
            build->preferred_base == XTA_KNOWN_BASE &&
            build->image_size == 0x00675000u &&
            build->pattern_matches == 1 &&
            match_rva + 13 == XTA_KNOWN_GATE - XTA_KNOWN_BASE) {
            actual_base = (uintptr_t)GetModuleHandleW(NULL);
            if (actual_base &&
                readable((void *)(actual_base + match_rva), sizeof gate_pattern) &&
                pattern_matches((unsigned char *)(actual_base + match_rva))) {
                /* Experimental opt-in profile for the user's exact SHA-256.
                   Still require the gate instruction, original threshold,
                   and controller-state pointers to be readable at runtime. */
                unsigned char *candidate_gate = (unsigned char *)(actual_base + match_rva + 13);
                uintptr_t candidate_threshold = 0;
                float candidate_value = 0.0f;
                uintptr_t candidate_wrapper = actual_base + XTA_KNOWN_WRAPPER - XTA_KNOWN_BASE;
                uintptr_t candidate_slots = actual_base + XTA_KNOWN_PLAYER_SLOTS - XTA_KNOWN_BASE;
                if (readable(candidate_gate, 13) &&
                    candidate_gate[0] == 0xD8 && candidate_gate[1] == 0x1D) {
                    memcpy(&candidate_threshold, candidate_gate + 2, sizeof(uint32_t));
                    if (candidate_threshold == actual_base + XTA_KNOWN_THRESHOLD - XTA_KNOWN_BASE &&
                        readable((void *)candidate_threshold, sizeof(candidate_value)) &&
                        readable((void *)candidate_wrapper, sizeof(void *)) &&
                        readable((void *)candidate_slots, 8)) {
                        memcpy(&candidate_value, (void *)candidate_threshold, sizeof candidate_value);
                        if (candidate_value == 0.75f) {
                            build->gate = candidate_gate;
                            build->threshold_address = candidate_threshold;
                            build->wrapper_cell = candidate_wrapper;
                            build->player_slots = candidate_slots;
                            build->retail_threshold = candidate_value;
                            build->status = XTA_SUPPORTED;
                            strcpy(build->reason, "EXPERIMENTAL exact-hash gate profile; runtime checks passed");
                            result = 1;
                        }
                    }
                }
                if (!result) strcpy(build->reason, "experimental profile rejected by runtime validation");
            }
        }
        if (!result && !strcmp(build->reason, "hash has no validated memory-layout profile")) {
            snprintf(build->reason, sizeof build->reason,
                "experimental static profile mismatch: image=%08X matches=%u gateRVA=%08X",
                build->image_size, build->pattern_matches, match_rva + 13);
        }
        goto finish;
    }
    if (build->file_size != XTA_KNOWN_SIZE ||
        build->pe_timestamp != XTA_KNOWN_TIMESTAMP ||
        build->preferred_base != XTA_KNOWN_BASE ||
        build->image_size != XTA_KNOWN_IMAGE_SIZE) {
        strcpy(build->reason, "known hash but PE metadata differs");
        goto finish;
    }
    if (build->pattern_matches != 1 || match_rva + 13 != XTA_KNOWN_GATE - XTA_KNOWN_BASE) {
        strcpy(build->reason, "movement signature missing, ambiguous or displaced");
        goto finish;
    }
    actual_base = (uintptr_t)GetModuleHandleW(NULL);
    if (!actual_base || match_rva > build->image_size - sizeof gate_pattern) {
        strcpy(build->reason, "loaded image does not contain signature RVA");
        goto finish;
    }
    if (!readable((void *)(actual_base + match_rva), sizeof gate_pattern) ||
        !pattern_matches((unsigned char *)(actual_base + match_rva))) {
        strcpy(build->reason, "loaded instructions differ from file signature");
        goto finish;
    }
    build->gate = (unsigned char *)(actual_base + match_rva + 13);
    memcpy(&threshold_address, build->gate + 2, sizeof threshold_address);
    if (threshold_address != actual_base + XTA_KNOWN_THRESHOLD - XTA_KNOWN_BASE ||
        !readable((void *)(uintptr_t)threshold_address, sizeof threshold)) {
        strcpy(build->reason, "threshold address is not the validated location");
        goto finish;
    }
    memcpy(&threshold, (void *)(uintptr_t)threshold_address, sizeof threshold);
    if (threshold != 0.75f) {
        strcpy(build->reason, "retail movement threshold differs from 0.75");
        goto finish;
    }
    build->wrapper_cell = actual_base + XTA_KNOWN_WRAPPER - XTA_KNOWN_BASE;
    build->player_slots = actual_base + XTA_KNOWN_PLAYER_SLOTS - XTA_KNOWN_BASE;
    if (!readable((void *)build->wrapper_cell, sizeof(void *)) ||
        !readable((void *)build->player_slots, 8)) {
        strcpy(build->reason, "controller state addresses are unavailable");
        goto finish;
    }
    build->retail_threshold = threshold;
    build->threshold_address = threshold_address;
    build->status = XTA_SUPPORTED;
    strcpy(build->reason, "known hash, unique signature and validated layout");
    result = 1;
    goto finish;
invalid_pe:
    strcpy(build->reason, "invalid or non-x86 PE image");
finish:
    HeapFree(GetProcessHeap(), 0, bytes);
    return result;
}

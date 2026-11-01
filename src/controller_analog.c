/* Compile the unchanged v0.1.0 analog core with Player1 configuration fallbacks.
   The legacy build still compiles analog_probe.c directly. */
#include "xml2_analog.h"
#include <wchar.h>

static UINT WINAPI player_config_int(LPCWSTR section,LPCWSTR key,INT fallback,LPCWSTR path) {
    UINT legacy=GetPrivateProfileIntW(section,key,fallback,path);
    if (!_wcsicmp(section,L"Controller")) {
        if (!_wcsicmp(key,L"DebugInput")) return GetPrivateProfileIntW(L"General",key,legacy,path);
        return GetPrivateProfileIntW(L"Player1",key,legacy,path);
    }
    return legacy;
}

static DWORD WINAPI player_config_string(LPCWSTR section,LPCWSTR key,LPCWSTR fallback,
                                         LPWSTR output,DWORD size,LPCWSTR path) {
    WCHAR legacy[32];
    if (!_wcsicmp(section,L"Controller") && !_wcsicmp(key,L"Deadzone")) {
        GetPrivateProfileStringW(section,key,fallback,legacy,32,path);
        return GetPrivateProfileStringW(L"Player1",key,legacy,output,size,path);
    }
    return GetPrivateProfileStringW(section,key,fallback,output,size,path);
}

#define GetPrivateProfileIntW player_config_int
#define GetPrivateProfileStringW player_config_string
#include "analog_probe.c"

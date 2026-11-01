/* Pass-through DirectInput 7 proxy; all seven exports retain their ABI. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
#include "xml2_analog.h"

static HMODULE self_module;
static HMODULE system_module;
static INIT_ONCE init_once = INIT_ONCE_STATIC_INIT;

void analog_log_event(const char *message) {
    WCHAR path[MAX_PATH];
    WCHAR *name;
    HANDLE file;
    DWORD written;
    DWORD length;
    OutputDebugStringA(message);
    length = GetEnvironmentVariableW(L"XML2_ANALOG_LOG", path, MAX_PATH);
    if (!length || length >= MAX_PATH) {
        if (!GetModuleFileNameW(self_module, path, MAX_PATH)) return;
        name = wcsrchr(path, L'\\');
        if (!name || (size_t)(name - path) + 16 >= MAX_PATH) return;
        lstrcpyW(name + 1, L"true-analog.log");
    }
    file = CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                       NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE) return;
    WriteFile(file, message, lstrlenA(message), &written, NULL);
    CloseHandle(file);
}

static BOOL CALLBACK load_system(PINIT_ONCE once, PVOID context, PVOID *ignored) {
    WCHAR path[MAX_PATH];
    UINT length;
    (void)once; (void)context; (void)ignored;
    length = GetSystemDirectoryW(path, MAX_PATH);
    if (!length || length + 12 >= MAX_PATH) return FALSE;
    lstrcatW(path, L"\\dinput.dll");
    system_module = LoadLibraryW(path);
    if (system_module) analog_log_event("DINPUT proxy: real system DLL loaded\r\n");
    return system_module != NULL;
}

static FARPROC real_export(const char *name) {
    if (!InitOnceExecuteOnce(&init_once, load_system, NULL, NULL)) return NULL;
    return GetProcAddress(system_module, name);
}

HRESULT WINAPI DirectInputCreateA(HINSTANCE instance, DWORD version,
                                                         void **out, void *outer) {
    typedef HRESULT (WINAPI *Fn)(HINSTANCE, DWORD, void **, void *);
    Fn fn = (Fn)real_export("DirectInputCreateA");
    return fn ? fn(instance, version, out, outer) : E_FAIL;
}

HRESULT WINAPI DirectInputCreateW(HINSTANCE instance, DWORD version,
                                                         void **out, void *outer) {
    typedef HRESULT (WINAPI *Fn)(HINSTANCE, DWORD, void **, void *);
    Fn fn = (Fn)real_export("DirectInputCreateW");
    return fn ? fn(instance, version, out, outer) : E_FAIL;
}

HRESULT WINAPI DirectInputCreateEx(HINSTANCE instance, DWORD version,
                                                          const IID *iid, void **out, void *outer) {
    typedef HRESULT (WINAPI *Fn)(HINSTANCE, DWORD, const IID *, void **, void *);
    Fn fn = (Fn)real_export("DirectInputCreateEx");
    HRESULT result = fn ? fn(instance, version, iid, out, outer) : E_FAIL;
    analog_log_event("DINPUT proxy: DirectInputCreateEx forwarded\r\n");
    if (SUCCEEDED(result)) analog_init(self_module);
    return result;
}

HRESULT WINAPI DllCanUnloadNow(void) {
    typedef HRESULT (WINAPI *Fn)(void);
    Fn fn = (Fn)real_export("DllCanUnloadNow");
    return fn ? fn() : E_FAIL;
}

HRESULT WINAPI DllGetClassObject(const CLSID *clsid,
                                                        const IID *iid, void **out) {
    typedef HRESULT (WINAPI *Fn)(const CLSID *, const IID *, void **);
    Fn fn = (Fn)real_export("DllGetClassObject");
    return fn ? fn(clsid, iid, out) : E_FAIL;
}

HRESULT WINAPI DllRegisterServer(void) {
    typedef HRESULT (WINAPI *Fn)(void);
    Fn fn = (Fn)real_export("DllRegisterServer");
    return fn ? fn() : E_FAIL;
}

HRESULT WINAPI DllUnregisterServer(void) {
    typedef HRESULT (WINAPI *Fn)(void);
    Fn fn = (Fn)real_export("DllUnregisterServer");
    return fn ? fn() : E_FAIL;
}

BOOL WINAPI DllMain(HINSTANCE module, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        self_module = module;
        DisableThreadLibraryCalls(module);
    }
    return TRUE;
}

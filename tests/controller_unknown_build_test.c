#define DIRECTINPUT_VERSION 0x0800
#define CINTERFACE
#include <dinput.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef HRESULT (WINAPI *CreateEx)(HINSTANCE,DWORD,REFIID,void **,LPUNKNOWN);

static DWORD *get_proc_iat(void) {
    uintptr_t base=(uintptr_t)GetModuleHandleW(NULL);
    IMAGE_DOS_HEADER *dos=(void *)base;
    IMAGE_NT_HEADERS32 *nt=(void *)(base+dos->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *imports=(void *)(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    unsigned i,j;
    for (i=0;imports[i].Name;i++) {
        IMAGE_THUNK_DATA32 *names=(void *)(base+imports[i].OriginalFirstThunk);
        IMAGE_THUNK_DATA32 *addresses=(void *)(base+imports[i].FirstThunk);
        if (!imports[i].OriginalFirstThunk) continue;
        for (j=0;names[j].u1.AddressOfData;j++) {
            IMAGE_IMPORT_BY_NAME *name;
            if (IMAGE_SNAP_BY_ORDINAL32(names[j].u1.Ordinal)) continue;
            name=(void *)(base+names[j].u1.AddressOfData);
            if (!strcmp((char *)name->Name,"GetProcAddress")) return &addresses[j].u1.Function;
        }
    }
    return NULL;
}

int main(int argc,char **argv) {
    DWORD *iat=get_proc_iat(),before;
    HMODULE proxy;
    CreateEx factory;
    void *manager=NULL;
    HRESULT hr;
    if (argc!=2 || !iat) return 1;
    before=*iat;
    proxy=LoadLibraryA(argv[1]);
    if (!proxy) { fprintf(stderr,"FAIL: LoadLibrary error=%lu\n",(unsigned long)GetLastError()); return 1; }
    factory=(CreateEx)GetProcAddress(proxy,"DirectInputCreateEx");
    if (!factory) return 1;
    hr=factory(GetModuleHandleW(NULL),0x0700,&IID_IDirectInput7A,&manager,NULL);
    if (*iat!=before) { fputs("FAIL: unsupported process IAT changed\n",stderr); return 1; }
    if (FAILED(hr) || !manager) { fprintf(stderr,"FAIL: system factory hr=0x%08lX\n",(unsigned long)hr); return 1; }
    ((ULONG (WINAPI *)(void *))(*(void ***)manager)[2])(manager);
    FreeLibrary(proxy);
    puts("PASS: unknown XMen2.exe fixture keeps original IAT and forwards the real system DirectInput factory.");
    return 0;
}

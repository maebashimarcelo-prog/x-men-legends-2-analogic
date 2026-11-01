/* Optional DirectInput lifecycle observer and runtime main-manager refresh. */
#define DIRECTINPUT_VERSION 0x0800
#define CINTERFACE
#define COBJMACROS
#include <dinput.h>
#include <cfgmgr32.h>
#include "controller_core.h"
#include "controller_model.h"
#include "controller_prompts.h"
#include "advanced_menu.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#define MAX_INTERFACES 64
#define MAX_INVENTORY 32

typedef HRESULT (WINAPI *Factory8)(HINSTANCE,DWORD,REFIID,void **,LPUNKNOWN);
typedef FARPROC (WINAPI *GetProcFn)(HMODULE,LPCSTR);
typedef HRESULT (WINAPI *EnumFn)(void *,DWORD,LPDIENUMDEVICESCALLBACKA,void *,DWORD);
typedef HRESULT (WINAPI *CreateFn)(void *,REFGUID,void **,LPUNKNOWN);
typedef HRESULT (WINAPI *CreateExFn)(void *,REFGUID,REFIID,void **,LPUNKNOWN);
typedef HRESULT (WINAPI *StateFn)(void *,DWORD,void *);
typedef HRESULT (WINAPI *SimpleFn)(void *);

typedef struct {
    void **dispatch;
    void *object;
    void *original[32];
    void *shadow[32];
    unsigned methods;
    unsigned version;
    int device;
    int keyboard;
    int mouse;
    GUID instance;
    HRESULT last_acquire;
    HRESULT last_state;
    HRESULT last_poll;
    int saw_state;
} Interface;
extern void *controller_forward_table[32];

typedef struct {
    DIDEVICEINSTANCEA device[MAX_INVENTORY];
    McIdentity identity[MAX_INVENTORY];
    unsigned count;
    void *observer;
} Inventory;
typedef struct { LPDIENUMDEVICESCALLBACKA callback; void *reference; unsigned count; unsigned version; } EnumContext;

static Interface interfaces[MAX_INTERFACES];
static Inventory previous_inventory;
static Interface *game_directinput8;
static GetProcFn original_get_proc;
static Factory8 original_factory;
static uintptr_t image_base;
static uintptr_t game_manager;
static uintptr_t wrapper_cell;
static DWORD next_inventory;
static unsigned game_enumerations;
static int prepared;
static int trace_enabled;
static int previous_slots_valid;
static uintptr_t previous_devices[10];
static unsigned char previous_attached[10];
static SRWLOCK inventory_lock=SRWLOCK_INIT;
static Inventory published_inventory;
static volatile LONG observer_started;
static HINSTANCE observer_instance;
static unsigned previous_binding_hash[4];
static int hotplug_enabled, refresh_validated, refreshing;
static uintptr_t auxiliary_refreshing,auxiliary_applied,auxiliary_factory;
static LONG auxiliary_generation;
static Inventory refresh_inventory;
static volatile LONG inventory_generation;
static LONG applied_generation;
static HANDLE inventory_changed;
static McCore core;
static unsigned preset_state[MC_PLAYERS];
static unsigned detached_mask;
static DWORD next_core_check;
static int core_configured;
static int observed_main_refresh;
static int binding_backend_ready;

static void emit_event(const char *format, va_list args) {
    char body[640], line[720];
    vsnprintf(body, sizeof body, format, args);
    snprintf(line, sizeof line, "Controller t=%lu %s\r\n", (unsigned long)GetTickCount(), body);
    analog_log_event(line);
}
static void event(const char *format, ...) {
    va_list args;
    if(!trace_enabled) return;
    va_start(args,format); emit_event(format,args); va_end(args);
}
static void status(const char *format, ...) {
    va_list args;
    va_start(args,format); emit_event(format,args); va_end(args);
}

static int readable(const void *pointer, size_t size) {
    MEMORY_BASIC_INFORMATION region;
    uintptr_t address = (uintptr_t)pointer;
    if (!pointer || VirtualQuery(pointer, &region, sizeof region) != sizeof region ||
        region.State != MEM_COMMIT || (region.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
    return size <= (uintptr_t)region.BaseAddress + region.RegionSize - address;
}

static const char *role(const Interface *it) {
    return it->keyboard ? "keyboard" : it->mouse ? "mouse" : it->device ? "controller" : "manager";
}

static void guid_string(const GUID *guid, char out[40]) {
    snprintf(out, 40, "%08lX-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X",
        (unsigned long)guid->Data1, guid->Data2, guid->Data3,
        guid->Data4[0],guid->Data4[1],guid->Data4[2],guid->Data4[3],
        guid->Data4[4],guid->Data4[5],guid->Data4[6],guid->Data4[7]);
}

static Interface *find_interface(void *object) {
    unsigned i;
    for (i=0;i<MAX_INTERFACES;i++) if (interfaces[i].object &&
        (interfaces[i].object==object || (void *)&interfaces[i]==object)) return &interfaces[i];
    return NULL;
}

static void *wrap_interface(void *object, unsigned version, int device, unsigned methods, const GUID *instance);
static void observe_inventory(void);
static DWORD WINAPI inventory_thread(void *unused);
static void pump_controller_core(void *buffer, DWORD size, uintptr_t caller);

static uintptr_t main_manager(void) {
    uintptr_t manager=0;
    if (readable((void *)wrapper_cell,sizeof manager)) manager=*(uintptr_t *)wrapper_cell;
    return readable((void *)manager,0x12A10u)?manager:0;
}

static HRESULT WINAPI query(void *object, REFIID iid, void **out) {
    Interface *it = find_interface(object);
    typedef HRESULT (WINAPI *Fn)(void *,REFIID,void **);
    HRESULT hr = ((Fn)it->original[0])(it->object,iid,out);
    if (SUCCEEDED(hr) && out && *out) {
        if (IsEqualGUID(iid,&IID_IDirectInput8A)) *out=wrap_interface(*out,8,0,11,NULL);
        else if (IsEqualGUID(iid,&IID_IDirectInput7A)) *out=wrap_interface(*out,7,0,10,NULL);
        else if (IsEqualGUID(iid,&IID_IDirectInputDevice8A)) *out=wrap_interface(*out,8,1,32,&it->instance);
        else if (IsEqualGUID(iid,&IID_IDirectInputDevice7A)) *out=wrap_interface(*out,7,1,29,&it->instance);
        else if (IsEqualGUID(iid,&IID_IDirectInputDevice2A)) *out=wrap_interface(*out,7,1,27,&it->instance);
        else if (IsEqualGUID(iid,&IID_IUnknown)) *out=wrap_interface(*out,it->version,it->device,it->methods,&it->instance);
    }
    return hr;
}

static ULONG WINAPI release(void *object) {
    Interface *it = find_interface(object);
    typedef ULONG (WINAPI *Fn)(void *);
    ULONG count = ((Fn)it->original[2])(it->object);
    if (!count) {
        event("DEVICE RELEASED api=DI%u role=%s object=%p",it->version,role(it),object);
        if (game_directinput8==it) game_directinput8=NULL;
        it->object=NULL;
    }
    return count;
}

static BOOL CALLBACK enumerated(const DIDEVICEINSTANCEA *device, void *reference) {
    EnumContext *ctx = reference;
    char instance[40], product[40];
    ctx->count++;
    guid_string(&device->guidInstance,instance);
    guid_string(&device->guidProduct,product);
    event("DEVICE ENUMERATED api=DI%u instance=%s product=%s type=0x%lX name=%.259s",
          ctx->version,instance,product,(unsigned long)device->dwDevType,device->tszProductName);
    return ctx->callback(device,ctx->reference);
}

static HRESULT WINAPI enumerate(void *object,DWORD type,LPDIENUMDEVICESCALLBACKA callback,void *reference,DWORD flags) {
    Interface *it=find_interface(object);
    EnumContext ctx={callback,reference,0,it->version};
    uintptr_t caller=(uintptr_t)__builtin_return_address(0);
    HRESULT hr;
    /* Reuse the game's entire refresh routine and callback on its input thread.
       This one refresh reads a worker snapshot instead of enumerating Windows
       on the frame thread. Ordinary game/menu enumerations still forward. */
    if (((refreshing && reference==(void *)game_manager && it==game_directinput8) ||
         (auxiliary_refreshing && reference==(void *)auxiliary_refreshing &&
          it==find_interface(*(void **)(auxiliary_refreshing+0x27e4)))) &&
        type==DI8DEVCLASS_GAMECTRL && flags==DIEDFL_ATTACHEDONLY &&
        (uintptr_t)callback==image_base+0x228E00u) {
        unsigned i;
        for (i=0;i<refresh_inventory.count;i++)
            if (callback(&refresh_inventory.device[i],reference)==DIENUM_STOP) break;
        return DI_OK;
    }
    event("GAME ENUM BEGIN api=DI%u type=%lu flags=0x%lX callback=%p ref=%p caller=%p",
          it->version,(unsigned long)type,(unsigned long)flags,(void *)callback,reference,(void *)caller);
    if (it->version==8 && type==DI8DEVCLASS_GAMECTRL) {
        game_enumerations++;
        if (caller==image_base+0x228E57u && readable(reference,0x129D0u)) {
            uintptr_t main=main_manager();
            event("GAME ENUM ROUTINE observed entry=%p ref=%p main=%p owner=%s record_new=%u count=%u",
                  (void *)(image_base+0x228E20u),reference,(void *)main,
                  !main?"not-published":main==(uintptr_t)reference?"MAIN":"AUXILIARY",
                  *(unsigned char *)((uintptr_t)reference+2),game_enumerations);
            if (main==(uintptr_t)reference) {
                game_manager=main; game_directinput8=it; observed_main_refresh=1;
            }
        }
    }
    hr=((EnumFn)it->original[4])(it->object,type,enumerated,&ctx,flags);
    event("GAME ENUM END api=DI%u count=%u hr=0x%08lX",it->version,ctx.count,(unsigned long)hr);
    return hr;
}

static HRESULT WINAPI create_device(void *object,REFGUID instance,void **out,LPUNKNOWN outer) {
    Interface *it=find_interface(object);
    HRESULT hr=((CreateFn)it->original[3])(it->object,instance,out,outer);
    char guid[40]; guid_string(instance,guid);
    event("DEVICE CREATED api=DI%u instance=%s object=%p hr=0x%08lX",it->version,guid,
          SUCCEEDED(hr)&&out?*out:NULL,(unsigned long)hr);
    if (SUCCEEDED(hr)&&out&&*out) *out=wrap_interface(*out,it->version,1,it->version==8?32:18,instance);
    return hr;
}

static HRESULT WINAPI create_device_ex(void *object,REFGUID instance,REFIID iid,void **out,LPUNKNOWN outer) {
    Interface *it=find_interface(object);
    HRESULT hr=((CreateExFn)it->original[9])(it->object,instance,iid,out,outer);
    char guid[40]; guid_string(instance,guid);
    event("DEVICE CREATED api=DI7-ex instance=%s object=%p hr=0x%08lX",guid,
          SUCCEEDED(hr)&&out?*out:NULL,(unsigned long)hr);
    if (SUCCEEDED(hr)&&out&&*out && IsEqualGUID(iid,&IID_IDirectInputDevice7A)) *out=wrap_interface(*out,7,1,29,instance);
    return hr;
}

static HRESULT WINAPI acquire(void *object) {
    Interface *it=find_interface(object);
    HRESULT hr=((SimpleFn)it->original[7])(it->object);
    if (it->last_acquire!=hr) {
        event("DEVICE ACQUIRED api=DI%u role=%s object=%p hr=0x%08lX",it->version,role(it),object,(unsigned long)hr);
        it->last_acquire=hr;
    }
    if (hotplug_enabled && hr==(HRESULT)DIERR_UNPLUGGED && !it->keyboard && !it->mouse) {
        uintptr_t main=main_manager();
        unsigned i;
        if (main) for (i=0;i<MC_GAME_SLOTS;i++)
            if (((void **)(main+0x0c))[i]==object) detached_mask|=1u<<i;
    }
    return hr;
}

static HRESULT WINAPI unacquire(void *object) {
    Interface *it=find_interface(object);
    HRESULT hr=((SimpleFn)it->original[8])(it->object);
    event("DEVICE UNACQUIRED api=DI%u role=%s object=%p hr=0x%08lX",it->version,role(it),object,(unsigned long)hr);
    return hr;
}

static HRESULT WINAPI get_state(void *object,DWORD size,void *out) {
    Interface *it=find_interface(object);
    HRESULT hr;
    if (it->keyboard && it->version==8) {
        pump_controller_core(out,size,(uintptr_t)__builtin_return_address(0));
        observe_inventory();
    }
    hr=((StateFn)it->original[9])(it->object,size,out);
    if(SUCCEEDED(hr) && it->version==8)
        mc_advanced_state(it->keyboard?0:it->mouse?1:2,out,size,(uintptr_t)__builtin_return_address(0),&it->instance);
    else if(FAILED(hr) && it->version==8 && !it->keyboard && !it->mouse && size==sizeof(DIJOYSTATE2))
        mc_advanced_lost(&it->instance);
    if(SUCCEEDED(hr) && it->version==8 && mc_prompts_enabled()) {
        uintptr_t main=main_manager();
        if(main && out==(void *)(main+0x25e4) && it->keyboard) mc_prompts_state(0,-1,out,size);
        else if(main && out==(void *)(main+0x4bc) && it->mouse) mc_prompts_state(1,-1,out,size);
        else if(main && !it->keyboard && !it->mouse) {
            unsigned s;
            for(s=0;s<MC_GAME_SLOTS;s++) if(out==(void *)(main+0x4f0+s*0x110)) { mc_prompts_state(2,(int)s,out,size); break; }
        }
    }
    if (!it->saw_state || it->last_state!=hr) {
        event("%s api=DI%u role=%s object=%p bytes=%lu hr=0x%08lX caller=%p",
              FAILED(hr)?"DEVICE LOST":"DEVICE STATE AVAILABLE",it->version,role(it),object,
              (unsigned long)size,(unsigned long)hr,__builtin_return_address(0));
        it->saw_state=1; it->last_state=hr;
        if (SUCCEEDED(hr) && !it->keyboard && !it->mouse && size==sizeof(DIJOYSTATE2)) {
            DIJOYSTATE2 *state=out;
            event("CONTROLLER FIRST READ instance_index_pending object=%p raw_xy=(%ld,%ld) main=%p",
                  object,(long)state->lX,(long)state->lY,(void *)main_manager());
        }
    }
    return hr;
}

static HRESULT WINAPI poll(void *object) {
    Interface *it=find_interface(object);
    HRESULT hr=((SimpleFn)it->original[25])(it->object);
    if (it->last_poll!=hr) {
        event("DEVICE POLL api=DI%u role=%s object=%p hr=0x%08lX",it->version,role(it),object,(unsigned long)hr);
        it->last_poll=hr;
    }
    return hr;
}

static void *wrap_interface(void *object,unsigned version,int device,unsigned methods,const GUID *instance) {
    Interface *it=find_interface(object);
    void **vtable;
    unsigned i;
    if (it) return it;
    if (!object || !readable(object,sizeof(void *))) return object;
    vtable=*(void ***)object;
    if (methods>32 || !readable(vtable,methods*sizeof(void *))) return object;
    for (i=0;i<MAX_INTERFACES;i++) if (!interfaces[i].object) break;
    if (i==MAX_INTERFACES) { status("TRACE CAPACITY exceeded; interface left unchanged"); return object; }
    it=&interfaces[i]; memset(it,0,sizeof *it);
    it->object=object; it->version=version; it->device=device; it->methods=methods;
    it->last_acquire=it->last_state=it->last_poll=E_PENDING;
    if (instance) {
        it->instance=*instance;
        it->keyboard=IsEqualGUID(instance,&GUID_SysKeyboard);
        it->mouse=IsEqualGUID(instance,&GUID_SysMouse);
    }
    memcpy(it->original,vtable,methods*sizeof(void *));
    memcpy(it->shadow,controller_forward_table,methods*sizeof(void *));
    it->shadow[0]=(void *)query; it->shadow[2]=(void *)release;
    if (device) {
        it->shadow[7]=(void *)acquire; it->shadow[8]=(void *)unacquire; it->shadow[9]=(void *)get_state;
        if (methods>25) it->shadow[25]=(void *)poll;
    } else {
        it->shadow[3]=(void *)create_device; it->shadow[4]=(void *)enumerate;
        if (version==7) it->shadow[9]=(void *)create_device_ex;
    }
    it->dispatch=it->shadow;
    return it;
}

static BOOL CALLBACK inventory_device(const DIDEVICEINSTANCEA *device,void *reference) {
    Inventory *inv=reference;
    if (inv->count<MAX_INVENTORY) inv->device[inv->count++]=*device;
    return DIENUM_CONTINUE;
}

static void read_identity(Inventory *inv,unsigned index) {
    const DIDEVICEINSTANCEA *record=&inv->device[index];
    McIdentity *identity=&inv->identity[index];
    void *device=NULL;
    void **vtable=*(void ***)inv->observer;
    unsigned i;
    for (i=0;i<previous_inventory.count;i++)
        if (IsEqualGUID(&previous_inventory.device[i].guidInstance,&record->guidInstance)) {
            *identity=previous_inventory.identity[i]; return;
        }
    memset(identity,0,sizeof *identity);
    identity->instance=record->guidInstance; identity->product=record->guidProduct;
    identity->native_type=record->dwDevType;
    memcpy(identity->name,record->tszProductName,sizeof identity->name);
    identity->name[sizeof identity->name-1]=0;
    if (SUCCEEDED(((CreateFn)vtable[3])(inv->observer,&record->guidInstance,&device,NULL)) && device) {
        typedef HRESULT (WINAPI *PropertyFn)(void *,REFGUID,DIPROPHEADER *);
        DIPROPDWORD ids={{sizeof ids,sizeof(DIPROPHEADER),0,DIPH_DEVICE},0};
        DIPROPGUIDANDPATH path={0};
        vtable=*(void ***)device;
        if (SUCCEEDED(((PropertyFn)vtable[6])(device,DIPROP_VIDPID,&ids.diph))) {
            identity->vid=LOWORD(ids.dwData); identity->pid=HIWORD(ids.dwData);
        }
        path.diph.dwSize=sizeof path; path.diph.dwHeaderSize=sizeof(DIPROPHEADER);
        path.diph.dwHow=DIPH_DEVICE;
        if (SUCCEEDED(((PropertyFn)vtable[6])(device,DIPROP_GUIDANDPATH,&path.diph))) {
            unsigned character;
            path.wszPath[MAX_PATH-1]=0;
            for (character=0;path.wszPath[character];character++)
                if (path.wszPath[character]>=L'a' && path.wszPath[character]<=L'z')
                    path.wszPath[character]-=L'a'-L'A';
            identity->xinput_path=wcsstr(path.wszPath,L"&IG_")!=NULL;
        }
        ((ULONG (WINAPI *)(void *))vtable[2])(device);
    }
    /* Standard DirectInput product GUID encodes VID/PID when property is absent. */
    if (!identity->vid && !memcmp(record->guidProduct.Data4,"\0\0PIDVID",8)) {
        identity->vid=LOWORD(record->guidProduct.Data1); identity->pid=HIWORD(record->guidProduct.Data1);
    }
}

static DWORD CALLBACK device_notification(HCMNOTIFICATION notification,void *context,
    CM_NOTIFY_ACTION action,CM_NOTIFY_EVENT_DATA *data,DWORD size) {
    (void)notification; (void)data; (void)size;
    if (action==CM_NOTIFY_ACTION_DEVICEINTERFACEARRIVAL || action==CM_NOTIFY_ACTION_DEVICEINTERFACEREMOVAL)
        SetEvent((HANDLE)context);
    return ERROR_SUCCESS;
}

static int inventory_has(const Inventory *inv,const GUID *guid) {
    unsigned i;
    for (i=0;i<inv->count;i++) if (IsEqualGUID(&inv->device[i].guidInstance,guid)) return 1;
    return 0;
}

static DWORD WINAPI inventory_thread(void *unused) {
    void *observer=NULL;
    void **vtable;
    HRESULT hr;
    HCMNOTIFICATION notification=NULL;
    HMODULE cfg=LoadLibraryExW(L"cfgmgr32.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
    typedef CONFIGRET (WINAPI *RegisterFn)(CM_NOTIFY_FILTER *,void *,PCM_NOTIFY_CALLBACK,PHCMNOTIFICATION);
    RegisterFn register_notification=cfg?(RegisterFn)GetProcAddress(cfg,"CM_Register_Notification"):NULL;
    int notified=0;
    (void)unused;
    hr=original_factory(observer_instance,0x0800,&IID_IDirectInput8A,&observer,NULL);
    if (FAILED(hr) || !observer) { status("WINDOWS INVENTORY observer creation failed hr=0x%08lX",(unsigned long)hr); return 0; }
    vtable=*(void ***)observer;
    inventory_changed=CreateEventW(NULL,FALSE,FALSE,NULL);
    if (register_notification && inventory_changed) {
        CM_NOTIFY_FILTER filter={0};
        filter.cbSize=sizeof filter; filter.Flags=CM_NOTIFY_FILTER_FLAG_ALL_INTERFACE_CLASSES;
        filter.FilterType=CM_NOTIFY_FILTER_TYPE_DEVICEINTERFACE;
        register_notification(&filter,inventory_changed,device_notification,&notification);
    }
    status("WINDOWS INVENTORY worker started; notifications=%s fallback_interval=5000ms",notification?"ON":"unavailable");
    for (;;) {
        Inventory current={0};
        unsigned i;
        DWORD begin=GetTickCount();
        current.observer=observer;
        hr=((EnumFn)vtable[4])(observer,DI8DEVCLASS_GAMECTRL,inventory_device,&current,DIEDFL_ATTACHEDONLY);
        if (SUCCEEDED(hr)) {
            int changed=current.count!=previous_inventory.count;
            for (i=0;i<current.count;i++) {
                read_identity(&current,i);
                if (!inventory_has(&previous_inventory,&current.device[i].guidInstance)) changed=1;
            }
            for (i=0;i<current.count;i++) if (!inventory_has(&previous_inventory,&current.device[i].guidInstance)) {
                char guid[40],product[40]; guid_string(&current.device[i].guidInstance,guid); guid_string(&current.device[i].guidProduct,product);
                event("WINDOWS DEVICE PRESENT instance=%s product=%s type=0x%lX name=%.259s enum_ms=%lu",
                      guid,product,(unsigned long)current.device[i].dwDevType,current.device[i].tszProductName,(unsigned long)(GetTickCount()-begin));
                event("DEVICE IDENTITY instance=%s vid=%04X pid=%04X xinput_path=%d detected_type=%s",
                      guid,current.identity[i].vid,current.identity[i].pid,current.identity[i].xinput_path,
                      mc_type_name(mc_classify(&current.identity[i])));
            }
            for (i=0;i<previous_inventory.count;i++) if (!inventory_has(&current,&previous_inventory.device[i].guidInstance)) {
                char guid[40]; guid_string(&previous_inventory.device[i].guidInstance,guid);
                event("WINDOWS DEVICE REMOVED instance=%s",guid);
            }
            previous_inventory=current;
            AcquireSRWLockExclusive(&inventory_lock);
            published_inventory=current;
            if (changed || notified || !inventory_generation) InterlockedIncrement(&inventory_generation);
            ReleaseSRWLockExclusive(&inventory_lock);
        }
        if (inventory_changed) {
            notified=WaitForSingleObject(inventory_changed,5000)==WAIT_OBJECT_0;
            if (notified) {
                /* Let a multi-interface HID arrival settle, outside gameplay. */
                Sleep(250);
                ResetEvent(inventory_changed);
            }
        } else { Sleep(5000); notified=0; }
    }
}

static void observe_bindings(void) {
    unsigned player;
    uintptr_t table=image_base+0x668F40u;
    if (!readable((void *)table,16*sizeof(uintptr_t))) return;
    for (player=0;player<4;player++) {
        uintptr_t controller=((uintptr_t *)table)[player];
        uintptr_t bindings=controller+0x18u;
        unsigned hash=2166136261u,pad_mask=0,row,slot;
        if (!controller || !readable((void *)bindings,42*4*12) || *(unsigned *)bindings!=42) continue;
        for (row=0;row<42;row++) for (slot=0;slot<4;slot++) {
            unsigned *entry=(unsigned *)(bindings+(row*4+slot)*12);
            hash=(hash^entry[1])*16777619u; hash=(hash^entry[2])*16777619u;
            if (entry[1]>=3 && entry[1]<=12) pad_mask|=1u<<(entry[1]-3);
        }
        if (hash!=previous_binding_hash[player]) {
            event("DEVICE ASSIGNED TO PLAYER observed_bindings player=%u pad_mask=0x%X binding_hash=%08X movement_alt=(%u:%u,%u:%u,%u:%u,%u:%u)",
                  player+1,pad_mask,hash,((unsigned *)bindings)[4],((unsigned *)bindings)[5],
                  ((unsigned *)bindings)[16],((unsigned *)bindings)[17],
                  ((unsigned *)bindings)[28],((unsigned *)bindings)[29],
                  ((unsigned *)bindings)[40],((unsigned *)bindings)[41]);
            previous_binding_hash[player]=hash;
        }
    }
}

static void observe_inventory(void) {
    DWORD now=GetTickCount();
    unsigned i;
    if (next_inventory && (LONG)(now-next_inventory)<0) return;
    next_inventory=now+1000;
    game_manager=main_manager();
    if (game_manager && readable((void *)game_manager,0x129D0u)) {
        uintptr_t *devices=(uintptr_t *)(game_manager+0x0C);
        unsigned char *attached=(unsigned char *)(game_manager+0x4E4);
        for (i=0;i<10;i++) if (!previous_slots_valid || devices[i]!=previous_devices[i] || attached[i]!=previous_attached[i]) {
            char guid[40]; guid_string((GUID *)(game_manager+0x27E8+i*16),guid);
            event("GAME DEVICE SLOT index=%u object=%p attached=%u instance=%s",i,(void *)devices[i],attached[i],guid);
            previous_devices[i]=devices[i]; previous_attached[i]=attached[i];
        }
        previous_slots_valid=1;
    }
    observe_bindings();
}

static int validate_refresh_profile(void) {
    static const unsigned char entry[]={0x8a,0x44,0x24,0x04,0x83,0xec,0x18,0x53,
                                         0x55,0x56,0x8b,0xf1,0x88,0x46,0x02,0x57};
    static const unsigned char setter[]={0x57,0x8b,0x7c,0x24,0x0c,0x85,0xff};
    IMAGE_DOS_HEADER *dos=(void *)image_base;
    IMAGE_NT_HEADERS32 *nt=(void *)(image_base+dos->e_lfanew);
    IMAGE_SECTION_HEADER *section=IMAGE_FIRST_SECTION(nt);
    unsigned matches=0,i;
    uintptr_t found=0;
    for (i=0;i<nt->FileHeader.NumberOfSections;i++) {
        unsigned j;
        unsigned char *start=(void *)(image_base+section[i].VirtualAddress);
        DWORD length=section[i].Misc.VirtualSize;
        if (!(section[i].Characteristics&IMAGE_SCN_MEM_EXECUTE) || length<sizeof entry || !readable(start,length)) continue;
        for (j=0;j<=length-sizeof entry;j++) if (!memcmp(start+j,entry,sizeof entry)) {
            matches++; found=(uintptr_t)(start+j);
        }
    }
    return matches==1 && found==image_base+0x228E20u &&
        *(unsigned char *)(image_base+0x228E40u)==0x68 &&
        *(uintptr_t *)(image_base+0x228E41u)==image_base+0x228E00u &&
        !memcmp((void *)(image_base+0x228E54u),"\xff\x52\x10",3) &&
        !memcmp((void *)(image_base+0x22861Eu),"\xff\x51\x24",3) &&
        !memcmp((void *)(image_base+0x2297A0u),setter,sizeof setter);
}

static int guid_slot(uintptr_t manager,const GUID *instance) {
    unsigned i;
    for (i=0;i<MC_GAME_SLOTS;i++)
        if (IsEqualGUID((GUID *)(manager+0x27E8+i*sizeof(GUID)),instance)) return (int)i;
    return -1;
}

static int reserved_slot(const GUID *instance) {
    unsigned i;
    for (i=0;i<core.count;i++) if (IsEqualGUID(&core.device[i].identity.instance,instance))
        return core.device[i].game_slot;
    return -1;
}

static void neutral_slot(uintptr_t manager,unsigned slot) {
    DIJOYSTATE2 *current=(void *)(manager+0x4F0u+slot*0x110u);
    DIJOYSTATE2 *previous=(void *)(manager+0xF90u+slot*0x110u);
    unsigned pov;
    memset(current,0,sizeof *current); memset(previous,0,sizeof *previous);
    for (pov=0;pov<4;pov++) current->rgdwPOV[pov]=previous->rgdwPOV[pov]=0xffffffffu;
    *(unsigned *)(manager+0x129CCu)&=~(1u<<slot);
}

static void capture_map(uintptr_t object,McBinding map[MC_ACTIONS][MC_BINDING_SLOTS]) {
    unsigned action,slot;
    for (action=0;action<MC_ACTIONS;action++) for (slot=0;slot<MC_BINDING_SLOTS;slot++) {
        unsigned *entry=(unsigned *)(object+(action*MC_BINDING_SLOTS+slot)*12);
        map[action][slot].kind=entry[1]; map[action][slot].code=entry[2];
    }
}

static int validate_binding_backend(void) {
    struct { unsigned head[4]; unsigned char bytes[0x968]; unsigned tail[4]; } buffer;
    unsigned char expected[sizeof buffer.bytes];
    McBinding map[MC_ACTIONS][MC_BINDING_SLOTS];
    McBindingWrite writes[MC_PRESET_WRITES];
    unsigned scenario,i,count;
    typedef void (__attribute__((thiscall)) *SetFn)(void *,unsigned,unsigned,unsigned,unsigned);
    /* Exercise the verified original setter on our own buffers, never a
       player's bindings. Trace mode validates fresh and explicit restore. */
    for (scenario=0;scenario<2;scenario++) {
        memset(&buffer,0,sizeof buffer);
        for (i=0;i<4;i++) buffer.head[i]=buffer.tail[i]=0x51a7e17u;
        *(unsigned *)buffer.bytes=MC_ACTIONS;
        if (scenario) {
            unsigned *entry=(unsigned *)buffer.bytes;
            entry[1]=1; entry[2]=0x11;
            entry=(unsigned *)(buffer.bytes+(7*MC_BINDING_SLOTS)*12);
            entry[1]=3; entry[2]=0x15;
            entry=(unsigned *)(buffer.bytes+(41*MC_BINDING_SLOTS+3)*12);
            entry[1]=4; entry[2]=0x16;
        }
        memcpy(expected,buffer.bytes,sizeof expected);
        capture_map((uintptr_t)buffer.bytes,map);
        count=mc_plan_preset(map,MC_GENERIC,0,(int)scenario,writes);
        if (count!=(scenario?25u:23u)) return 0;
        for (i=0;i<count;i++) {
            McBindingWrite *write=&writes[i];
            unsigned *entry=(unsigned *)(expected+(write->action*MC_BINDING_SLOTS+write->slot)*12);
            entry[1]=write->kind; entry[2]=write->code; entry[3]=0;
            if (write->kind>=3) *(unsigned *)(expected+0x964)=write->kind;
            ((SetFn)(image_base+0x2297A0u))(buffer.bytes,write->action,write->slot,write->kind,write->code);
        }
        if (memcmp(expected,buffer.bytes,sizeof expected)) return 0;
        for (i=0;i<4;i++) if (buffer.head[i]!=0x51a7e17u || buffer.tail[i]!=0x51a7e17u) return 0;
    }
    return 1;
}

static void apply_console_defaults(void) {
    uintptr_t table=image_base+0x668F40u;
    unsigned player;
    if (binding_backend_ready!=1 || !readable((void *)table,16*sizeof(uintptr_t))) return;
    for (player=0;player<MC_PLAYERS;player++) {
        McPlayer *p=&core.player[player];
        McDevice *device;
        McType type;
        uintptr_t sets[3];
        McBinding map[MC_ACTIONS][MC_BINDING_SLOTS];
        McBindingWrite writes[MC_PRESET_WRITES];
        unsigned bank,i,count;
        int ready=1;
        if (!p->console_default || p->device_index<0) continue;
        device=&core.device[p->device_index];
        if (!device->connected || device->game_slot<0 ||
            !((void **)(game_manager+0x0c))[device->game_slot]) continue;
        type=p->type_override==MC_UNKNOWN?device->detected_type:p->type_override;
        for (bank=0;bank<3;bank++) {
            unsigned index=player+(bank==0?0:bank==1?4:12);
            uintptr_t object=((uintptr_t *)table)[index];
            sets[bank]=object?object+0x18u:0;
            if (!readable((void *)sets[bank],0x968u) || *(unsigned *)sets[bank]!=MC_ACTIONS) ready=0;
        }
        if (!ready) continue;
        capture_map(sets[0],map);
        if (!mc_can_install_preset(map,p->restore_defaults)) {
            if (preset_state[player]!=1) status("PRESET PRESERVED player=%u existing custom or keyboard/mouse alternate bindings",player+1);
            preset_state[player]=1; continue;
        }
        if (type==MC_PLAYSTATION) {
            if (preset_state[player]!=2) status("PRESET DEFERRED player=%u Sony native HID mapping not hardware-validated; existing bindings preserved",player+1);
            preset_state[player]=2; continue;
        }
        /* Every destination is checked before the first setter call. */
        for (bank=1;bank<3;bank++) {
            capture_map(sets[bank],map);
            if (!mc_can_install_preset(map,p->restore_defaults)) ready=0;
        }
        if (!ready) continue;
        for (bank=0;bank<3;bank++) {
            capture_map(sets[bank],map);
            count=mc_plan_preset(map,type,device->game_slot,p->restore_defaults,writes);
            for (i=0;i<count;i++) {
                typedef void (__attribute__((thiscall)) *SetFn)(void *,unsigned,unsigned,unsigned,unsigned);
                McBindingWrite *write=&writes[i];
                ((SetFn)(image_base+0x2297A0u))((void *)sets[bank],write->action,write->slot,
                                             write->kind,write->code);
            }
        }
        p->restore_defaults=0;
        preset_state[player]=3;
        status("PRESET APPLIED player=%u preset=ConsoleDefault pad=%d type=%s actions=%u banks=master,working,menu runtime_only=1",
              player+1,device->game_slot,mc_type_name(type),23u);
    }
}

static void update_model(const Inventory *inventory) {
    unsigned i,player;
    int before[MC_PLAYERS];
    for (player=0;player<MC_PLAYERS;player++) before[player]=core.player[player].device_index;
    mc_begin_inventory(&core);
    for (i=0;i<inventory->count;i++) {
        int slot=guid_slot(game_manager,&inventory->device[i].guidInstance);
        mc_device_observe(&core,&inventory->identity[i],slot);
    }
    mc_assign_players(&core);
    for (player=0;player<MC_PLAYERS;player++) {
        int index=core.player[player].device_index;
        if (index>=0 && (index!=before[player] || !core_configured)) {
            McDevice *device=&core.device[index];
            char guid[40]; guid_string(&device->identity.instance,guid);
            status("DEVICE ASSIGNED TO PLAYER player=%u registry_index=%d game_slot=%d instance=%s connected=%d detected=%s effective=%s",
                  player+1,index,device->game_slot,guid,device->connected,mc_type_name(device->detected_type),
                  mc_type_name(core.player[player].type_override==MC_UNKNOWN?device->detected_type:core.player[player].type_override));
        }
    }
    core_configured=1;
}

static void refresh_main(const Inventory *inventory,unsigned lost) {
    typedef void (__attribute__((thiscall)) *RefreshFn)(void *,int);
    unsigned i,pass,occupied=0;
    DWORD begin=GetTickCount();
    refresh_inventory=*inventory;
    /* A confirmed unplug invalidates that COM object even when a worker's
       most recent snapshot predates the removal. Drop it from this refresh. */
    for (i=0;i<refresh_inventory.count;) {
        int slot=guid_slot(game_manager,&refresh_inventory.device[i].guidInstance);
        if (slot>=0 && (lost&(1u<<slot))) {
            unsigned last=--refresh_inventory.count;
            refresh_inventory.device[i]=refresh_inventory.device[last];
            refresh_inventory.identity[i]=refresh_inventory.identity[last];
        } else i++;
    }
    for (i=0;i<MC_GAME_SLOTS;i++) if (((void **)(game_manager+0x0c))[i]) occupied|=1u<<i;
    for (i=0;i<core.count;i++) if (core.device[i].game_slot>=0)
        occupied|=1u<<(unsigned)core.device[i].game_slot;
    /* First preserve existing GUID slots, then reserve new primary pads before
       supplemental endpoints. DirectInput enumeration order is not identity. */
    for (pass=0;pass<2;pass++) for (i=0;i<refresh_inventory.count;i++) {
        int slot=guid_slot(game_manager,&refresh_inventory.device[i].guidInstance);
        int primary=mc_primary_device(&refresh_inventory.identity[i]);
        unsigned free_slot;
        if (primary!=(pass==0)) continue;
        if (slot<0) slot=reserved_slot(&refresh_inventory.device[i].guidInstance);
        if (slot<0) {
            for (free_slot=0;free_slot<MC_GAME_SLOTS;free_slot++) if (!(occupied&(1u<<free_slot))) break;
            if (free_slot==MC_GAME_SLOTS) {
                status("DEVICE AVAILABLE no free game slot; ten original slots/reservations exhausted"); continue;
            }
            slot=(int)free_slot; occupied|=1u<<free_slot;
        }
        *(GUID *)(game_manager+0x27E8+(unsigned)slot*sizeof(GUID))=refresh_inventory.device[i].guidInstance;
        mc_device_observe(&core,&refresh_inventory.identity[i],slot);
    }
    /* An unassigned overflow device must never reach the original callback's
       first-null fallback, which would overwrite a reserved disconnected slot. */
    for (i=0;i<refresh_inventory.count;) {
        if (reserved_slot(&refresh_inventory.device[i].guidInstance)<0) {
            unsigned last=--refresh_inventory.count;
            refresh_inventory.device[i]=refresh_inventory.device[last];
            refresh_inventory.identity[i]=refresh_inventory.identity[last];
        } else i++;
    }
    refreshing=1;
    ((RefreshFn)(image_base+0x228E20u))((void *)game_manager,1);
    refreshing=0;
    for (i=0;i<core.count;i++) {
        int slot=core.device[i].game_slot;
        if (slot<0) continue;
        if (!((void **)(game_manager+0x0c))[slot]) {
            /* Original refresh cleared its GUID; retain our session reservation. */
            *(GUID *)(game_manager+0x27E8+(unsigned)slot*sizeof(GUID))=core.device[i].identity.instance;
            neutral_slot(game_manager,(unsigned)slot);
        }
    }
    update_model(&refresh_inventory);
    status("HOTPLUG MAIN REFRESH generation=%ld connected_records=%u confirmed_lost_mask=0x%X duration_ms=%lu",
          (long)applied_generation,refresh_inventory.count,lost,(unsigned long)(GetTickCount()-begin));
}

static void pump_controller_core(void *buffer,DWORD size,uintptr_t caller) {
    LONG generation;
    Inventory inventory;
    uintptr_t manager;
    DWORD now;
    unsigned lost;
    if ((!hotplug_enabled && !mc_prompts_enabled() && !mc_advanced_enabled()) || !refresh_validated || !observed_main_refresh || refreshing ||
        size!=256 || caller!=image_base+0x228621u) return;
    now=GetTickCount();
    if (next_core_check && (LONG)(now-next_core_check)<0) return;
    next_core_check=now+200;
    manager=main_manager();
    if (!manager || buffer!=(void *)(manager+0x25E4u)) return;
    game_manager=manager;
    game_directinput8=find_interface(*(void **)(manager+0x27E4u));
    if (!game_directinput8 || game_directinput8->device || game_directinput8->version!=8) return;
    if (!binding_backend_ready) {
        if (trace_enabled) {
            binding_backend_ready=validate_binding_backend()?1:-1;
            status("PRESET BACKEND %s fresh/restore own-buffer setter check; player bindings untouched",
                  binding_backend_ready==1?"PASS":"REFUSED");
        } else {
            binding_backend_ready=1;
            status("PRESET BACKEND profile validated; own-buffer selftest disabled with ControllerTrace=0");
        }
    }
    generation=InterlockedCompareExchange(&inventory_generation,0,0);
    if (!generation) return;
    if (generation!=applied_generation || detached_mask) {
        AcquireSRWLockShared(&inventory_lock);
        inventory=published_inventory;
        generation=inventory_generation;
        ReleaseSRWLockShared(&inventory_lock);
        lost=detached_mask; detached_mask=0;
        if (!core_configured) update_model(&inventory);
        applied_generation=generation;
        if(hotplug_enabled) refresh_main(&inventory,lost);
        else update_model(&inventory);
    }
    /* The Advanced list owns a second native input manager. Refresh it only
       on entry/inventory changes, using the same worker snapshot and original
       callback. No Windows enumeration is introduced on the UI frame thread. */
    if(hotplug_enabled) {
        uintptr_t auxiliary=mc_advanced_manager(),factory;
        Interface *input;
        if(auxiliary && auxiliary!=manager) {
            factory=*(uintptr_t *)(auxiliary+0x27e4); input=find_interface((void *)factory);
            if(input && !input->device && input->version==8 &&
               (auxiliary!=auxiliary_applied || factory!=auxiliary_factory || auxiliary_generation!=applied_generation)) {
                typedef void (__attribute__((thiscall)) *AdvancedRefreshFn)(void *,int);
                unsigned slot,j;
                for(slot=0;slot<MC_GAME_SLOTS;slot++) {
                    GUID *guid=(void *)(auxiliary+0x27e8+slot*16); int present=0;
                    for(j=0;j<refresh_inventory.count;j++) if(IsEqualGUID(guid,&refresh_inventory.device[j].guidInstance)) present=1;
                    if(!present) neutral_slot(auxiliary,slot);
                }
                auxiliary_refreshing=auxiliary;
                ((AdvancedRefreshFn)(image_base+0x228e20))((void *)auxiliary,1);
                auxiliary_refreshing=0;
                auxiliary_applied=auxiliary; auxiliary_factory=factory; auxiliary_generation=applied_generation;
                status("HOTPLUG ADVANCED REFRESH generation=%ld manager=%p snapshot_records=%u",(long)applied_generation,(void *)auxiliary,refresh_inventory.count);
            }
        } else { auxiliary_applied=0; auxiliary_factory=0; auxiliary_generation=0; }
    }
    /* This checks bounded data only, no allocation, enumeration or frame logs. */
    apply_console_defaults();
}

static HRESULT WINAPI create8(HINSTANCE instance,DWORD version,REFIID iid,void **out,LPUNKNOWN outer) {
    HRESULT hr=original_factory(instance,version,iid,out,outer);
    event("DIRECTINPUT CREATED api=DI8 version=0x%lX hr=0x%08lX object=%p",(unsigned long)version,
          (unsigned long)hr,SUCCEEDED(hr)&&out?*out:NULL);
    if (SUCCEEDED(hr)&&out&&*out&&IsEqualGUID(iid,&IID_IDirectInput8A)) {
        HANDLE thread;
        *out=wrap_interface(*out,8,0,11,NULL);
        if (!InterlockedCompareExchange(&observer_started,1,0)) {
            observer_instance=instance;
            thread=CreateThread(NULL,0,inventory_thread,NULL,0,NULL);
            if (thread) CloseHandle(thread);
            else status("WINDOWS INVENTORY worker could not start");
        }
    }
    return hr;
}

static FARPROC WINAPI get_proc(HMODULE module,LPCSTR name) {
    FARPROC result=original_get_proc(module,name);
    if ((uintptr_t)name>65535 && !strcmp(name,"DirectInput8Create") && result) {
        HMODULE expected=LoadLibraryExW(L"dinput8.dll",NULL,LOAD_LIBRARY_SEARCH_SYSTEM32);
        int is_system=expected && expected==module;
        if (expected) FreeLibrary(expected);
        if (is_system) {
                original_factory=(Factory8)result;
                event("DI8 FACTORY OBSERVED system=%p caller=%p",(void *)result,__builtin_return_address(0));
                return (FARPROC)create8;
        }
    }
    return result;
}

static int arm_factory_observer(void) {
    IMAGE_DOS_HEADER *dos=(void *)image_base;
    IMAGE_NT_HEADERS32 *nt=(void *)(image_base+dos->e_lfanew);
    IMAGE_IMPORT_DESCRIPTOR *imports=(void *)(image_base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
    unsigned i,j;
    for (i=0;imports[i].Name;i++) {
        IMAGE_THUNK_DATA32 *names=(void *)(image_base+imports[i].OriginalFirstThunk);
        IMAGE_THUNK_DATA32 *addresses=(void *)(image_base+imports[i].FirstThunk);
        if (!imports[i].OriginalFirstThunk) continue;
        for (j=0;names[j].u1.AddressOfData;j++) {
            IMAGE_IMPORT_BY_NAME *name;
            DWORD old,restored;
            if (IMAGE_SNAP_BY_ORDINAL32(names[j].u1.Ordinal)) continue;
            name=(void *)(image_base+names[j].u1.AddressOfData);
            if (strcmp((char *)name->Name,"GetProcAddress")) continue;
            original_get_proc=(GetProcFn)(uintptr_t)addresses[j].u1.Function;
            if (original_get_proc!=GetProcAddress || !VirtualProtect(&addresses[j].u1.Function,4,PAGE_READWRITE,&old)) return 0;
            InterlockedExchange((LONG volatile *)&addresses[j].u1.Function,(LONG)(uintptr_t)get_proc);
            VirtualProtect(&addresses[j].u1.Function,4,old,&restored);
            status("CORE ARMED GetProcAddress IAT=%p; HotPlug=%s",&addresses[j].u1.Function,hotplug_enabled?"ON":"OFF");
            return 1;
        }
    }
    return 0;
}

void controller_core_prepare(HMODULE module) {
    WCHAR path[MAX_PATH],*name;
    XtaBuild build;
    if (prepared++) return;
    if (!GetModuleFileNameW(module,path,MAX_PATH)) return;
    name=wcsrchr(path,L'\\'); if (!name || (size_t)(name-path)+16>=MAX_PATH) return;
    lstrcpyW(name+1,L"true-analog.ini");
    trace_enabled=GetPrivateProfileIntW(L"General",L"ControllerTrace",0,path)==1;
    hotplug_enabled=GetPrivateProfileIntW(L"General",L"HotPlug",0,path)==1;
    if (!trace_enabled && !hotplug_enabled && GetPrivateProfileIntW(L"General",L"DynamicPrompts",0,path)!=1 &&
        GetPrivateProfileIntW(L"General",L"PromptTrace",0,path)!=1 &&
        GetPrivateProfileIntW(L"General",L"AdvancedTrace",0,path)!=1 &&
        GetPrivateProfileIntW(L"General",L"AdvancedController",0,path)!=1) return;
    status("GAME START " XTA_PRODUCT " v" XTA_VERSION "; HotPlug=%s",hotplug_enabled?"ON":"OFF");
    if (!analog_identify_build(&build) || build.status!=XTA_SUPPORTED) {
        status("TRACE REFUSED unsupported build; no IAT or COM changes"); return;
    }
    image_base=(uintptr_t)GetModuleHandleW(NULL);
    wrapper_cell=build.wrapper_cell;
    refresh_validated=validate_refresh_profile();
    if (hotplug_enabled && !refresh_validated) {
        status("HOTPLUG REFUSED refresh/signature profile changed; observer only"); hotplug_enabled=0;
    }
    mc_core_init(&core);
    mc_prompts_prepare(image_base,path,&core);
    {
        unsigned player;
        for (player=0;player<MC_PLAYERS;player++) {
            WCHAR section[16],type[32],preset[32];
            int fallback=player==0?GetPrivateProfileIntW(L"Controller",L"ControllerIndex",0,path):-1;
            swprintf(section,16,L"Player%u",player+1);
            core.player[player].controller_index=GetPrivateProfileIntW(section,L"ControllerIndex",fallback,path);
            GetPrivateProfileStringW(section,L"ControllerType",L"Auto",type,32,path);
            core.player[player].type_override=!_wcsicmp(type,L"Xbox")?MC_XBOX:
                !_wcsicmp(type,L"PlayStation")?MC_PLAYSTATION:!_wcsicmp(type,L"Generic")?MC_GENERIC:MC_UNKNOWN;
            GetPrivateProfileStringW(section,L"Preset",L"ConsoleDefault",preset,32,path);
            core.player[player].console_default=!_wcsicmp(preset,L"ConsoleDefault");
            core.player[player].restore_defaults=GetPrivateProfileIntW(section,L"RestoreConsoleDefaults",0,path)==1;
            event("PLAYER CONFIG player=%u controller_index=%d type_override=%s preset=%s restore=%d",
                  player+1,core.player[player].controller_index,mc_type_name(core.player[player].type_override),
                  core.player[player].console_default?"ConsoleDefault":"None",core.player[player].restore_defaults);
            if (player && GetPrivateProfileIntW(section,L"TrueAnalogMovement",0,path)==1)
                status("TRUE ANALOG DEFERRED player=%u multiplayer resolver path not hardware-validated",player+1);
        }
    }
    mc_advanced_prepare(image_base,path,&core);
    status("REFRESH PROFILE %s entry=%p callback=%p; True Analog core remains P1 validated",
          refresh_validated?"VALIDATED":"REFUSED",(void *)(image_base+0x228E20u),(void *)(image_base+0x228E00u));
    if (!arm_factory_observer()) status("TRACE REFUSED factory observer unavailable");
}

void controller_core_created7(const IID *iid,void *object) {
    if ((!trace_enabled && !hotplug_enabled) || !image_base) return;
    event("DIRECTINPUT CREATED api=DI7 object=%p",object);
    (void)iid;
}

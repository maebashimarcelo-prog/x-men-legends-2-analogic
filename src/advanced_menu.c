/* Context adapter for the retained PC Advanced editor. Optional diagnostics
   retain every original body; gameplay input and executable files are untouched. */
#include "advanced_menu.h"
#include "controller_prompts.h"
#include "prompt_render.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static uintptr_t image,active_list;
static int tracing,armed,enabled;
static unsigned notes;
static const McCore *controller_core;
static void emit(const char *fmt,va_list ap) {
    char body[640],line[720];
    vsnprintf(body,sizeof body,fmt,ap);
    snprintf(line,sizeof line,"Advanced t=%lu %s\r\n",(unsigned long)GetTickCount(),body);
    analog_log_event(line);
}
static void note(const char *fmt,...) {
    va_list ap; if(!tracing || notes++>=512) return;
    va_start(ap,fmt); emit(fmt,ap); va_end(ap);
}
static void status(const char *fmt,...) {
    va_list ap; va_start(ap,fmt); emit(fmt,ap); va_end(ap);
}
static int readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && VirtualQuery(p,&m,sizeof m)==sizeof m && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_GUARD|PAGE_NOACCESS)) && n<=(uintptr_t)m.BaseAddress+m.RegionSize-a;
}
static uintptr_t word(uintptr_t at) { return readable((void *)at,4)?*(uintptr_t *)at:0; }
static uintptr_t root_for(uintptr_t list) {
    uintptr_t root=word(image+0x66ad34);
    return root && readable((void *)root,0x54) && readable((void *)list,0x6e0) &&
        word(list)==image+0x2a57ec && word(list+0x2c)==root &&
        (!enabled || *(unsigned char *)(list+0x30))?root:0;
}
typedef struct { void *target,*trampoline; unsigned count; unsigned char bytes[16]; } Hook;
static Hook hooks[6];
static unsigned hook_count;
static int unique(uintptr_t address,const unsigned char *signature,unsigned size) {
    IMAGE_DOS_HEADER *dos=(void *)image;
    IMAGE_NT_HEADERS32 *pe=(void *)(image+dos->e_lfanew);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(pe);
    unsigned i,matches=0; uintptr_t found=0;
    for(i=0;i<pe->FileHeader.NumberOfSections;i++) if(sections[i].Characteristics&IMAGE_SCN_MEM_EXECUTE) {
        unsigned j,length=sections[i].Misc.VirtualSize; unsigned char *start=(void *)(image+sections[i].VirtualAddress);
        for(j=0;j+size<=length;j++) if(!memcmp(start+j,signature,size)) { matches++; found=(uintptr_t)start+j; }
    }
    return matches==1 && found==address;
}
static int rollback(void) {
    int result=1;
    while(hook_count) {
        Hook *h=&hooks[--hook_count]; DWORD old,restored;
        if(!VirtualProtect(h->target,h->count,PAGE_EXECUTE_READWRITE,&old)) { result=0; continue; }
        memcpy(h->target,h->bytes,h->count);
        if(!FlushInstructionCache(GetCurrentProcess(),h->target,h->count)) result=0;
        if(!VirtualProtect(h->target,h->count,old,&restored)) result=0;
        if(result) VirtualFree(h->trampoline,0,MEM_RELEASE);
    }
    return result;
}
static void *detour(uintptr_t address,unsigned stolen,void *replacement) {
    unsigned char *tramp,*target=(void *)address,patch[16]; int32_t rel; DWORD old,restored;
    tramp=VirtualAlloc(NULL,32,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!tramp) return NULL;
    memcpy(tramp,target,stolen); tramp[stolen]=0xe9;
    rel=(int32_t)(address+stolen-(uintptr_t)(tramp+stolen+5)); memcpy(tramp+stolen+1,&rel,4);
    if(!VirtualProtect(tramp,32,PAGE_EXECUTE_READ,&old) || !FlushInstructionCache(GetCurrentProcess(),tramp,32)) {
        VirtualFree(tramp,0,MEM_RELEASE); return NULL;
    }
    if(!VirtualProtect(target,stolen,PAGE_EXECUTE_READWRITE,&old)) { VirtualFree(tramp,0,MEM_RELEASE); return NULL; }
    hooks[hook_count]=(Hook){target,tramp,stolen,{0}}; memcpy(hooks[hook_count].bytes,target,stolen); hook_count++;
    memset(patch,0x90,stolen); patch[0]=0xe9;
    rel=(int32_t)((uintptr_t)replacement-address-5); memcpy(patch+1,&rel,4); memcpy(target,patch,stolen);
    if(!FlushInstructionCache(GetCurrentProcess(),target,stolen)) {
        memcpy(target,tramp,stolen); FlushInstructionCache(GetCurrentProcess(),target,stolen);
        VirtualProtect(target,stolen,old,&restored); hook_count--; VirtualFree(tramp,0,MEM_RELEASE); return NULL;
    }
    if(!VirtualProtect(target,stolen,old,&restored)) note("PROTECTION restore failed at %p; trampoline retained",target);
    return tramp;
}
typedef void (__attribute__((thiscall)) *TickFn)(void *,float);
typedef void (__attribute__((thiscall)) *DispatchFn)(void *,unsigned,uintptr_t,uintptr_t);
typedef void (__attribute__((thiscall)) *SelectFn)(void *,int,int);
typedef void (__attribute__((thiscall)) *NotifyFn)(void *,unsigned);
static TickFn tick_original;
static DispatchFn dispatch_original;
static SelectFn select_original;
static NotifyFn notify_original;
typedef void (__attribute__((thiscall)) *BodyFn)(void *);
typedef const char *(__attribute__((thiscall)) *NameFn)(void *,unsigned,unsigned);
static BodyFn draw_original;
static NameFn name_original;
/* Menu commands are independent of physical keyboard codes. Only this PC UI
   protocol adapter translates them to its observed internal dispatch messages. */
typedef enum { MENU_UP,MENU_DOWN,MENU_LEFT,MENU_RIGHT,MENU_ACCEPT,MENU_BACK } MenuCommand;
static uintptr_t session_list,draw_list;
static unsigned pending_accept,pending_back;
static int consumed_accept,consumed_back;
static unsigned char ui_keys[256];
static DWORD key_time[256];
static DIJOYSTATE2 ui_pad;
static int pad_valid,entry_gate,connection_lost;
static int capturing(void) { return (int)word(active_list+0x1c8)>=0 && (int)word(active_list+0x1cc)>=0; }
static const McDevice *primary(void) {
    int index=controller_core->player[0].device_index;
    return index>=0 && index<(int)controller_core->count?&controller_core->device[index]:NULL;
}
static McType effective_type(const McDevice *d) {
    return controller_core->player[0].type_override==MC_UNKNOWN?d->detected_type:controller_core->player[0].type_override;
}
static int held(McControl control,const DIJOYSTATE2 *pad,McType type) {
    unsigned code=mc_native_code(control,type);
    return code>=0x15 && code<0x95 && (pad->rgbButtons[code-0x15]&128)!=0;
}
static int neutral(const DIJOYSTATE2 *pad) {
    unsigned i; double x=pad->lX,y=pad->lY;
    if(x*x+y*y>40000 || (pad->rgdwPOV[0]&0xffff)!=0xffff) return 0;
    for(i=0;i<128;i++) if(pad->rgbButtons[i]&128) return 0;
    return 1;
}
static void command(MenuCommand cmd) {
    static const unsigned protocol[]={0x26,0x28,0x25,0x27,0x0d,0x1b};
    uintptr_t root=root_for(active_list);
    if(!root || *(unsigned char *)(root+0x44) || word(root+0x48) || capturing()) return;
    /* Directions and mouse-selected binding capture retain their native paths.
       Secondary native modal dialogs already support the original pad route. */
    if(cmd!=MENU_ACCEPT && cmd!=MENU_BACK) return;
    if(word(image+0x66ad38) || word(image+0x66ad3c) || word(image+0x66ad40) || word(image+0x66ad44)) return;
    if(cmd==MENU_ACCEPT) consumed_accept=1;
    if(cmd==MENU_BACK) consumed_back=1;
    note("COMMAND %u original Advanced dispatch",(unsigned)cmd);
    dispatch_original((void *)root,0x101,protocol[cmd],0);
}
static void __attribute__((thiscall)) list_draw(void *object) {
    draw_list=enabled && (uintptr_t)object==active_list && root_for(active_list)?active_list:0;
    draw_original(object); draw_list=0;
}
static const char *__attribute__((thiscall)) binding_name(void *object,unsigned kind,unsigned code) {
    const char *result=name_original(object,kind,code),*logical=NULL;
    unsigned i; McType type=MC_GENERIC;
    if(!draw_list || (uintptr_t)__builtin_return_address(0)!=image+0x225b78 ||
       (uintptr_t)object!=word(draw_list+0x54c)) return result;
    if(kind>=3 && kind<3+MC_GAME_SLOTS && mc_prompts_controller_source(0)) {
        const GUID *guid=(void *)((uintptr_t)object+0x27e8+(kind-3)*16);
        for(i=0;i<controller_core->count;i++) if(IsEqualGUID(guid,&controller_core->device[i].identity.instance)) {
            type=controller_core->player[0].type_override==MC_UNKNOWN?controller_core->device[i].detected_type:controller_core->player[0].type_override; break;
        }
        logical=mc_prompts_menu_binding(code,type,(unsigned)word(draw_list+0x544));
    }
    if(logical) result=logical;
    return result;
}
typedef struct { uintptr_t list,root,mouse; unsigned focus_mask; int row,col,page,scroll; } Snapshot;
static Snapshot previous;
static Snapshot snapshot(uintptr_t list) {
    Snapshot s={0}; uintptr_t root=root_for(list),children; unsigned i,count;
    if(!root) return s;
    s.list=list; s.root=root; s.mouse=word(root+0x10);
    s.row=(int)word(list+0x1c8); s.col=(int)word(list+0x1cc);
    s.page=(int)word(list+0x544); s.scroll=(int)word(list+0x1d0);
    children=word(root+0x14); count=(unsigned)word(root+0x24);
    if(count>32 || !readable((void *)children,count*4)) return s;
    for(i=0;i<count;i++) {
        uintptr_t child=word(children+i*4);
        if(readable((void *)child,0x1ad) && *(unsigned char *)(child+0x1ac)) s.focus_mask|=1u<<i;
    }
    return s;
}
static void observed(uintptr_t list,const char *stage) {
    if(!tracing) return;
    Snapshot s=snapshot(list);
    if(s.list && memcmp(&s,&previous,sizeof s)) {
        note("STATE stage=%s list=%p root=%p mouse_focus=%p focus_mask=%08X capture=%d:%d player_page=%d scroll=%d glyph_active=%d source_controller=%d",
            stage,(void *)s.list,(void *)s.root,(void *)s.mouse,s.focus_mask,s.row,s.col,s.page,s.scroll,
            pg_render_active(),mc_prompts_controller_source(0));
        previous=s;
    }
}
static void __attribute__((thiscall)) tick(void *object,float dt) {
    uintptr_t list=(uintptr_t)object;
    if(root_for(list)) {
        active_list=list;
        if(enabled && session_list!=list) {
            session_list=list; pad_valid=0; entry_gate=1;
            pending_accept=pending_back=0; consumed_accept=consumed_back=0;
            memset(ui_keys,0,sizeof ui_keys); memset(key_time,0,sizeof key_time);
        }
        observed(list,"before tick");
    }
    tick_original(object,dt);
    if(enabled && root_for(list)) {
        const McDevice *d=primary();
        if(!d || !d->connected || connection_lost) {
            pad_valid=0; pending_accept=pending_back=0; entry_gate=1;
        }
        if(pending_back) { pending_back=0; pending_accept=0; command(MENU_BACK); }
        else if(pending_accept) { pending_accept=0; command(MENU_ACCEPT); }
    }
    observed(list,"after tick");
}
static void __attribute__((thiscall)) dispatch(void *object,unsigned message,uintptr_t wparam,uintptr_t lparam) {
    uintptr_t root=root_for(active_list);
    if((uintptr_t)object==root && root && message!=0x200)
        note("DISPATCH message=%03X wparam=%08lX lparam=%08lX caller=%p",message,(unsigned long)wparam,(unsigned long)lparam,__builtin_return_address(0));
    if(enabled && root && message==0x101 && (uintptr_t)__builtin_return_address(0)==image+0x2192cc &&
       ((wparam==0x0d && consumed_accept && !(ui_keys[28]&128) && (DWORD)(GetTickCount()-key_time[28])>120) ||
        (wparam==0x1b && consumed_back && !(ui_keys[1]&128) && (DWORD)(GetTickCount()-key_time[1])>120))) return;
    dispatch_original(object,message,wparam,lparam);
    if((uintptr_t)object==root && root) {
        observed(active_list,"after dispatch");
        if(word(root+0x48)) { session_list=0; pad_valid=0; pending_accept=pending_back=0; }
    }
}
static void __attribute__((thiscall)) select_cell(void *object,int row,int col) {
    if(root_for((uintptr_t)object)) note("SELECT row=%d col=%d caller=%p",row,col,__builtin_return_address(0));
    select_original(object,row,col);
    observed((uintptr_t)object,"after select");
}
static void __attribute__((thiscall)) notify_widget(void *object,unsigned event) {
    uintptr_t widget=(uintptr_t)object,root=root_for(active_list);
    if(root && readable(object,0x70) && word(widget+0x2c)==root)
        note("NOTIFY widget=%p id=%u event=%u callback=%p focus=%u caller=%p",object,(unsigned)word(widget+0x5c),event,
            (void *)word(widget+0x68),readable((void *)(widget+0x1ac),1)?*(unsigned char *)(widget+0x1ac):0,__builtin_return_address(0));
    notify_original(object,event);
}
static unsigned char previous_keys[256],previous_buttons[10][128];
static unsigned previous_pov[10];
static int previous_direction[10];
void mc_advanced_state(int role,const void *state,unsigned size,uintptr_t caller,const GUID *instance) {
    uintptr_t root,manager,out=(uintptr_t)state;
    unsigned i; int slot=-1;
    if(!armed) return;
    if((role==0 && caller!=image+0x228621) || (role==1 && caller!=image+0x2286bf) ||
       (role==2 && caller!=image+0x228835)) return;
    root=root_for(active_list);
    if(!root) { active_list=0; session_list=0; pad_valid=0; return; }
    manager=word(active_list+0x54c);
    if(!manager || !state) return;
    if(enabled && role==0 && size==256) {
        const unsigned char *keys=state; DWORD now=GetTickCount();
        for(i=0;i<256;i++) if((keys[i]&128)!=(ui_keys[i]&128)) {
            key_time[i]=now;
        }
        memcpy(ui_keys,keys,256);
    }
    if(enabled && role==2 && size==sizeof(DIJOYSTATE2) && instance) {
        const McDevice *d=primary(); const DIJOYSTATE2 *pad=state;
        if(d && d->connected && IsEqualGUID(instance,&d->identity.instance)) {
            McType type=effective_type(d);
            connection_lost=0;
            if(!held(MC_SOUTH,pad,type)) consumed_accept=0;
            if(!held(MC_EAST,pad,type)) consumed_back=0;
            if(!pad_valid) { ui_pad=*pad; pad_valid=1; }
            if(entry_gate) { if(neutral(pad)) entry_gate=0; }
            else if(!capturing()) {
                if(held(MC_EAST,pad,type) && !held(MC_EAST,&ui_pad,type)) pending_back=1;
                if(held(MC_SOUTH,pad,type) && !held(MC_SOUTH,&ui_pad,type)) pending_accept=1;
            } else pending_accept=pending_back=0;
            ui_pad=*pad;
        }
    }
    if(!tracing) return;
    if(role==0 && size==256 && out==manager+0x25e4) {
        const unsigned char *keys=state;
        for(i=0;i<256;i++) if((keys[i]&128)!=(previous_keys[i]&128))
            note("KEY raw=%u down=%d manager=%p caller=%p",i,(keys[i]&128)!=0,(void *)manager,(void *)caller);
        memcpy(previous_keys,keys,256);
    } else if(role==2 && size==sizeof(DIJOYSTATE2)) {
        const DIJOYSTATE2 *pad=state; int dir=0;
        for(i=0;i<MC_GAME_SLOTS;i++) if(out==manager+0x4f0+i*0x110) { slot=(int)i; break; }
        if(slot<0) return;
        for(i=0;i<128;i++) if((pad->rgbButtons[i]&128)!=(previous_buttons[slot][i]&128)) {
            McControl control; McType type=MC_GENERIC; unsigned d;
            for(d=0;d<controller_core->count;d++) if(controller_core->device[d].game_slot==slot) {
                type=controller_core->player[0].type_override==MC_UNKNOWN?controller_core->device[d].detected_type:controller_core->player[0].type_override; break;
            }
            control=MC_CONTROL_COUNT;
            for(d=MC_SOUTH;d<MC_CONTROL_COUNT;d++) if(mc_native_code((McControl)d,type)==i+0x15) { control=(McControl)d; break; }
            note("PAD slot=%d raw_button=%u logical=%u down=%d manager=%p caller=%p",slot,i,(unsigned)control,
                (pad->rgbButtons[i]&128)!=0,(void *)manager,(void *)caller);
        }
        memcpy(previous_buttons[slot],pad->rgbButtons,128);
        if(pad->lX>600) dir|=1; if(pad->lX< -600) dir|=2;
        if(pad->lY>600) dir|=4; if(pad->lY< -600) dir|=8;
        if(previous_direction[slot]!=dir || previous_pov[slot]!=pad->rgdwPOV[0]) {
            note("DIRECTION slot=%d xy=%ld,%ld pov=%lu quadrant=%d",slot,pad->lX,pad->lY,(unsigned long)pad->rgdwPOV[0],dir);
            previous_direction[slot]=dir; previous_pov[slot]=pad->rgdwPOV[0];
        }
    }
}
void mc_advanced_prepare(uintptr_t base,const WCHAR *ini,const McCore *core) {
    static const unsigned char tp[]={0xd9,0x44,0x24,0x04,0x55,0x8b,0xe9,0xd8,0x85,0x18,0x05,0x00,0x00};
    static const unsigned char dp[]={0x8b,0x44,0x24,0x04,0x83,0xec,0x38,0x3d,0x05,0x02,0x00,0x00};
    static const unsigned char sp[]={0x8b,0x81,0xc4,0x01,0x00,0x00,0x53,0x33,0xd2,0x85,0xc0,0x57};
    static const unsigned char np[]={0x8b,0x41,0x68,0x85,0xc0,0x74,0x12,0x8b,0x51,0x6c};
    static const unsigned char gp[]={0x83,0xec,0x44,0x53,0x55,0x56,0x8b,0xf1,0x8b,0x56,0x24};
    static const unsigned char fp[]={0x8b,0x54,0x24,0x08,0x83,0xfa,0xff,0x56,0x8b,0xf1};
    image=base; controller_core=core;
    tracing=GetPrivateProfileIntW(L"General",L"AdvancedTrace",0,ini)==1;
    enabled=GetPrivateProfileIntW(L"General",L"AdvancedController",0,ini)==1;
    if(!tracing && !enabled) return;
    if(image!=0x400000u || !unique(image+0x226a20,tp,sizeof tp) || !unique(image+0x221ea0,dp,sizeof dp) ||
        !unique(image+0x2265a0,sp,sizeof sp) || !unique(image+0x21fd10,np,sizeof np) ||
        word(image+0x2a5810)!=image+0x226a20 || word(image+0x2a582c)!=image+0x226ad0) {
        status("REFUSED changed/nonunique Advanced signatures/layout; original behavior retained"); return;
    }
    if(enabled && (!unique(image+0x225840,gp,sizeof gp) ||
       !unique(image+0x2281f0,fp,sizeof fp) || word(image+0x2a580c)!=image+0x225840 ||
       word(image+0x2a57f0)!=image+0x224600)) {
        status("REFUSED changed Advanced display layout; other modules retained"); enabled=0; return;
    }
    tick_original=(TickFn)detour(image+0x226a20,7,(void *)tick);
    dispatch_original=(DispatchFn)detour(image+0x221ea0,7,(void *)dispatch);
    select_original=(SelectFn)detour(image+0x2265a0,6,(void *)select_cell);
    notify_original=(NotifyFn)detour(image+0x21fd10,5,(void *)notify_widget);
    armed=tick_original && dispatch_original && select_original && notify_original;
    if(armed && enabled) {
        draw_original=(BodyFn)detour(image+0x225840,5,(void *)list_draw);
        name_original=(NameFn)detour(image+0x2281f0,7,(void *)binding_name);
        armed=draw_original && name_original;
    }
    if(!armed) { int restored; enabled=0; restored=rollback(); status("REFUSED incomplete Advanced transaction; restoration_verified=%d",restored); return; }
    status("ARMED observe_only=%d tick=00626A20 dispatch=00621EA0 select=006265A0 notify=0061FD10 controller=%d; transitions only",!enabled,enabled);
}
uintptr_t mc_advanced_manager(void) {
    uintptr_t manager=armed && enabled && root_for(active_list)?word(active_list+0x54c):0;
    return readable((void *)manager,0x12a10)?manager:0;
}
int mc_advanced_enabled(void) { return armed && enabled; }
void mc_advanced_lost(const GUID *instance) {
    const McDevice *d;
    if(!armed || !enabled || !root_for(active_list) || !instance || !(d=primary()) ||
       !IsEqualGUID(instance,&d->identity.instance)) return;
    if(!connection_lost) note("PAD unavailable; pending commands cleared; neutral input required on return");
    connection_lost=1; pad_valid=0; entry_gate=1;
    pending_accept=pending_back=0;
}

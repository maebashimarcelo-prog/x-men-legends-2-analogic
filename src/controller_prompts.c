/* Independent, fail-closed retail prompt instrumentation. Original bodies run. */
#include "controller_prompts.h"
#include "prompt_model.h"
#include "prompt_render.h"
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static uintptr_t exe;
static int tracing;
static int dynamic,glyphs,armed,tutorial_controller_text;
static const McCore *controller_core;
static PgStyle styles[MC_PLAYERS];
static PgInput sources[MC_PLAYERS];
static float deadzones[MC_PLAYERS];
static unsigned char previous_keys[256],previous_mouse[8];
static DIJOYSTATE2 previous_pads[MC_GAME_SLOTS];
static unsigned char previous_pad_valid[MC_GAME_SLOTS];
static unsigned label_changes[MC_PLAYERS][MC_ACTIONS];
static int selected_player=-1;
static McBinding selected_binding;
static char styled_label[MC_PLAYERS][32];
static McType selected_type;
static void note(const char *format, ...) {
    char body[512],line[580]; va_list ap;
    va_start(ap,format); vsnprintf(body,sizeof body,format,ap); va_end(ap);
    snprintf(line,sizeof line,"Prompts t=%lu %s\r\n",(unsigned long)GetTickCount(),body);
    analog_log_event(line);
}
static int readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && VirtualQuery(p,&m,sizeof m)==sizeof m && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) && n<=(uintptr_t)m.BaseAddress+m.RegionSize-a;
}
/* Only instruction-aligned, relocation-free prologues from this SHA profile.
   All patterns must have exactly one executable-section occurrence. */
static int unique_at(uintptr_t base,uintptr_t at,const unsigned char *s,size_t n) {
    IMAGE_DOS_HEADER *d=(void *)base; IMAGE_NT_HEADERS32 *pe=(void *)(base+d->e_lfanew);
    IMAGE_SECTION_HEADER *sections=IMAGE_FIRST_SECTION(pe); unsigned i,count=0;
    uintptr_t found=0;
    for(i=0;i<pe->FileHeader.NumberOfSections;i++) if(sections[i].Characteristics&IMAGE_SCN_MEM_EXECUTE) {
        unsigned char *start=(void *)(base+sections[i].VirtualAddress); size_t j,len=sections[i].Misc.VirtualSize;
        for(j=0;j+n<=len;j++) if(!memcmp(start+j,s,n)) { count++; found=(uintptr_t)(start+j); }
    }
    return count==1 && found==at;
}
static int gfx_layout(uintptr_t base) {
    static const unsigned char dev[]={0x8b,0x86,0x44,0x01,0x00,0x00};
    static const unsigned char array[]={0x8b,0x86,0xf0,0x01,0x00,0x00};
    IMAGE_DOS_HEADER *dos=(void *)base; IMAGE_NT_HEADERS32 *pe;
    int32_t offset;
    if(!readable(dos,sizeof *dos) || dos->e_magic!=IMAGE_DOS_SIGNATURE || dos->e_lfanew<0 ||
       dos->e_lfanew>0x100000) return 0;
    pe=(void *)(base+(unsigned)dos->e_lfanew);
    if(!readable(pe,sizeof *pe) || pe->Signature!=IMAGE_NT_SIGNATURE ||
       pe->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR32_MAGIC || pe->OptionalHeader.SizeOfImage<0x35480 ||
       !readable((void *)(base+0x353c4),sizeof array) || !readable((void *)(base+0x35402),5) ||
       !readable((void *)(base+0x35477),sizeof dev)) return 0;
    if(memcmp((void *)(base+0x35477),dev,sizeof dev) || memcmp((void *)(base+0x353c4),array,sizeof array) ||
       *(unsigned char *)(base+0x35402)!=0xe8) return 0;
    memcpy(&offset,(void *)(base+0x35403),4);
    return base+0x35407+(intptr_t)offset==base+0x34e60;
}
typedef struct { void *target,*trampoline; unsigned count; unsigned char bytes[16]; } Hook;
static Hook hooks[7];
static unsigned hook_count;
static int rollback_hooks(void) {
    int complete=1;
    while(hook_count) {
        Hook *h=&hooks[--hook_count]; DWORD old,restored;
        if(VirtualProtect(h->target,h->count,PAGE_EXECUTE_READWRITE,&old)) {
            memcpy(h->target,h->bytes,h->count);
            if(!FlushInstructionCache(GetCurrentProcess(),h->target,h->count)) complete=0;
            if(!VirtualProtect(h->target,h->count,old,&restored)) complete=0;
            /* Retain executable storage if restoration could not be verified. */
            if(complete && h->trampoline) VirtualFree(h->trampoline,0,MEM_RELEASE);
        } else complete=0;
    }
    return complete;
}
static void *detour(uintptr_t base,uintptr_t address,const unsigned char *signature,size_t size,unsigned stolen,void *replacement) {
    unsigned char *trampoline,*target=(void *)address,patch[16]; DWORD old,restored; int32_t rel;
    if(stolen<5 || stolen>sizeof patch || !unique_at(base,address,signature,size)) return NULL;
    trampoline=VirtualAlloc(NULL,32,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    if(!trampoline) return NULL;
    memcpy(trampoline,target,stolen); trampoline[stolen]=0xe9;
    rel=(int32_t)(address+stolen-(uintptr_t)(trampoline+stolen+5)); memcpy(trampoline+stolen+1,&rel,4);
    if(!VirtualProtect(trampoline,32,PAGE_EXECUTE_READ,&old)) { VirtualFree(trampoline,0,MEM_RELEASE); return NULL; }
    FlushInstructionCache(GetCurrentProcess(),trampoline,32);
    memset(patch,0x90,stolen); patch[0]=0xe9; rel=(int32_t)((uintptr_t)replacement-address-5); memcpy(patch+1,&rel,4);
    if(!VirtualProtect(target,stolen,PAGE_EXECUTE_READWRITE,&old)) { VirtualFree(trampoline,0,MEM_RELEASE); return NULL; }
    hooks[hook_count]=(Hook){target,trampoline,stolen,{0}};
    memcpy(hooks[hook_count].bytes,target,stolen); hook_count++;
    memcpy(target,patch,stolen);
    if(!FlushInstructionCache(GetCurrentProcess(),target,stolen)) {
        memcpy(target,trampoline,stolen); FlushInstructionCache(GetCurrentProcess(),target,stolen);
        VirtualProtect(target,stolen,old,&restored); hook_count--; VirtualFree(trampoline,0,MEM_RELEASE); return NULL;
    }
    VirtualProtect(target,stolen,old,&restored);
    return trampoline;
}
typedef const char *(__attribute__((cdecl)) *LabelFn)(unsigned);
typedef void (__attribute__((thiscall)) *ReadFn)(void *,unsigned,int,unsigned *,unsigned *);
typedef void (__attribute__((thiscall)) *TextFn)(void *,const WCHAR *,void *,int,int,void *,void *,void *);
typedef void (__attribute__((thiscall)) *QuadFn)(void *,float,float,float,float,float,float,float,float);
typedef void (__attribute__((thiscall)) *DrawFn)(void *,unsigned,int,int);
typedef void (__attribute__((thiscall)) *UpdateFn)(void *,unsigned,int);
static LabelFn label_original;
static ReadFn read_original;
static TextFn text_original;
static QuadFn quad_original;
static DrawFn draw_original;
static UpdateFn update_original;
typedef const char *(__attribute__((cdecl)) *LocalizeFn)(const char *);
typedef const char *(__attribute__((thiscall)) *FieldFn)(void *,const char *,const char *);
static unsigned tutorial_notes;
static char controller_help[2048];
/* Called only by the checked CPopupDialog::create PC-hint callsite. Reader is
   its live stack local. No asset is extracted, replaced or modified. */
const char *__attribute__((cdecl)) mc_tutorial_observe(const char *key,void *reader) {
    const char *pc=((LocalizeFn)(exe+0x229bf0))(key),*asset=NULL;
    uintptr_t saved;
    int use_controller=dynamic && tutorial_controller_text && sources[0].connected && sources[0].source==PG_CONTROLLER;
    if(!readable(reader,12)) return pc;
    saved=*(uintptr_t *)((char *)reader+8);
    asset=((FieldFn)(exe+0x164b70))(reader,(const char *)(exe+0x285cbc),(const char *)(exe+0x281968));
    *(uintptr_t *)((char *)reader+8)=saved;
    if(tracing && tutorial_notes++<8 && readable(key,64) && readable(pc,128) && readable(asset,128))
        note("TUTORIAL key=%.60s reader=%p pc=%.110s asset=%.110s select=%s",key,reader,pc,asset,
             use_controller?"asset":"PC");
    if(use_controller && readable(asset,1) && *asset) {
        unsigned n;
        if(!readable(key,15) || (strcmp(key,"switching_hint") && strcmp(key,"automap_hint") && strcmp(key,"potion_hint"))) return asset;
        /* Bound and validate the loaded field before the scoped token pass.
           Unsupported prose retains the original field and renderer fallback. */
        for(n=0;n<sizeof controller_help;n++) {
            if(!readable(asset+n,1)) break;
            if(!asset[n]) {
                if(pg_help_tokens(key,asset,controller_help,sizeof controller_help)) {
                    if(tracing) note("HELP key=%.14s verified control labels -> native action tokens",key);
                    return controller_help;
                }
                break;
            }
        }
        return asset;
    }
    return pc;
}
static const char *__attribute__((naked,cdecl)) tutorial_gate(const char *key __attribute__((unused))) {
    __asm__("leal 0x1c(%esp), %eax\n\tpushl %eax\n\tpushl 8(%esp)\n\tcall _mc_tutorial_observe\n\taddl $8, %esp\n\tret");
}
static int tutorial_hook(void) {
    /* Validated CALL target, ordinary reader branch and both constant fields. */
    static const unsigned char site[]={0x8d,0x4c,0x24,0x58,0x51,0xe8,0x8a,0xdb,0x03,0x00,0x83,0xc4,0x04,0xeb,0x13,
        0x68,0x68,0x19,0x68,0x00,0x68,0xbc,0x5c,0x68,0x00,0x8d,0x4c,0x24,0x1c};
    static const unsigned char field[]={0x56,0x8b,0xf1,0x8b,0x0e,0x85,0xc9,0x74,0x16};
    static const unsigned char localize[]={0x8b,0x0d,0x74,0xb1,0xa6,0x00,0x85,0xc9,0x74,0x13};
    unsigned char *target=(void *)(exe+0x1ec061),patch[5]; DWORD old,restored; int32_t offset;
    if(!unique_at(exe,exe+0x1ec05c,site,sizeof site) || !unique_at(exe,exe+0x164b70,field,sizeof field) ||
       !unique_at(exe,exe+0x229bf0,localize,sizeof localize) ||
       strcmp((const char *)(exe+0x285cbc),"text") || *(char *)(exe+0x281968)) return 0;
    patch[0]=0xe8; offset=(int32_t)((uintptr_t)tutorial_gate-(uintptr_t)target-5); memcpy(patch+1,&offset,4);
    if(!VirtualProtect(target,5,PAGE_EXECUTE_READWRITE,&old)) return 0;
    hooks[hook_count]=(Hook){target,NULL,5,{0}}; memcpy(hooks[hook_count].bytes,target,5); hook_count++;
    memcpy(target,patch,5);
    if(!FlushInstructionCache(GetCurrentProcess(),target,5)) {
        memcpy(target,hooks[--hook_count].bytes,5); FlushInstructionCache(GetCurrentProcess(),target,5);
        VirtualProtect(target,5,old,&restored); return 0;
    }
    VirtualProtect(target,5,old,&restored); return 1;
}
static unsigned current_action;
static int in_label,in_text;
static unsigned row_seen[MC_ACTIONS],action_seen[256],text_notes,quad_notes,draw_notes,glyph_run_seen[16],glyph_run_count;
static const char *__attribute__((cdecl)) label(unsigned action) {
    const char *result; int presentation=(uintptr_t)__builtin_return_address(0)==exe+0xbd73eu;
    current_action=action; if(presentation) in_label++; selected_player=-1;
    result=label_original(action); if(presentation) in_label--;
    if(tracing && action<256 && !action_seen[action]++ && readable(result,64))
        note("LABEL action=%u caller=%p original=%.60s",action,__builtin_return_address(0),result);
    if(dynamic && presentation && selected_player>=0) {
        McControl control; PgStyle style=pg_effective_style(styles[selected_player],selected_type);
        if(pg_native_control(selected_binding.code,selected_type,&control)) {
            int glyph=glyphs?pg_render_code((style==PG_PLAYSTATION?24u:0u)+(unsigned)control):0;
            char *out=styled_label[selected_player];
            if(glyph) { out[0]=(char)(glyph&255); out[1]=(char)((unsigned)glyph>>8); out[2]=0; }
            else snprintf(out,32,"[%s]",pg_control_name(control,style));
            if(tracing && action<256) {
                unsigned row=action%MC_ACTIONS,value=selected_binding.code|(unsigned)style<<16|(glyph?0x1000000u:0);
                if(label_changes[selected_player][row]!=value) {
                    label_changes[selected_player][row]=value;
                    note("DYNAMIC player=%d action=%u kind=%u code=%u logical=%s style=%s output=%s",selected_player+1,
                         action,selected_binding.kind,selected_binding.code,pg_control_name(control,PG_XBOX),
                         style==PG_PLAYSTATION?"PlayStation":"Xbox",glyph?"glyph":"text fallback");
                }
            }
            return out;
        }
    }
    return result;
}
static int player_for_object(void *object) {
    unsigned i; uintptr_t *tables=(void *)(exe+0x668f40);
    if(!readable(tables,16*sizeof *tables)) return -1;
    for(i=0;i<16;i++) if(tables[i] && (uintptr_t)object==tables[i]+0x18) return (int)(i%4);
    return -1;
}
static int connected_slot(unsigned player,McType *type) {
    int index=controller_core->player[player].device_index;
    const McDevice *device;
    if(index<0 || index>=(int)controller_core->count) return -1;
    device=&controller_core->device[index];
    if(!device->connected) return -1;
    *type=controller_core->player[player].type_override==MC_UNKNOWN?device->detected_type:controller_core->player[player].type_override;
    return device->game_slot;
}
static void __attribute__((thiscall)) binding(void *object,unsigned row,int slot,unsigned *kind,unsigned *code) {
    read_original(object,row,slot,kind,code);
    if(tracing && in_label && row<MC_ACTIONS && row_seen[row]++<4)
        note("BIND action=%u row=%u object=%p slot=%d kind=%u code=%u caller=%p",current_action,row,object,slot,
             kind?*kind:0,code?*code:0,__builtin_return_address(0));
    if(dynamic && in_label && row<MC_ACTIONS) {
        int player=player_for_object(object); McType type;
        McBinding map[4],choice; int pad; unsigned i;
        if(player<0 || sources[player].source!=PG_CONTROLLER || (pad=connected_slot((unsigned)player,&type))<0 ||
           !readable((char *)object+row*48,48)) return;
        /* Exactly one action's four live slots, never the full binding map per frame. */
        for(i=0;i<4;i++) { unsigned *b=(void *)((char *)object+row*48+i*12); map[i]=(McBinding){b[1],b[2]}; }
        if(!pg_select_binding(map,pad,&choice)) return;
        if(kind) *kind=choice.kind; if(code) *code=choice.code;
        selected_player=player; selected_binding=choice; selected_type=type;
    }
}
static void __attribute__((thiscall)) text(void *owner,const WCHAR *string,void *writer,int x,int y,void *style,void *pen,void *scale) {
    unsigned i; int bracket=0;
    if(tracing && glyphs && readable(string,256) && glyph_run_count<16) {
        unsigned index,hash=2166136261u; int contains=0;
        for(index=0;index<48;index++) { int code=pg_render_code(index);
            if(code && string[0]==(WCHAR)(code&255) && string[1]==(WCHAR)(code>>8)) contains=1;
        }
        if(contains) {
            char sample[385]; unsigned n=0,seen=0;
            for(i=0;i<64 && string[i];i++) { hash=(hash^string[i])*16777619u; n+=(unsigned)snprintf(sample+n,sizeof sample-n,"%04X ",string[i]); }
            sample[n]=0;
            for(i=0;i<glyph_run_count;i++) if(glyph_run_seen[i]==hash) seen=1;
            if(!seen) { glyph_run_seen[glyph_run_count++]=hash;
                note("GLYPHRUN owner=%p font=%u xy=%d,%d style=%p wchar=%s",owner,
                     readable((char *)owner+0x2140c,4)?*(unsigned *)((char *)owner+0x2140c):99,x,y,style,sample);
            }
        }
    }
    if(tracing && readable(string,128)) for(i=0;i<64 && string[i];i++) if(string[i]=='[') bracket=1;
    if(bracket && text_notes++<12) {
        char sample[65]; for(i=0;i<64 && string[i];i++) sample[i]=string[i]<128?(char)string[i]:'?'; sample[i]=0;
        note("TEXT stack_string=%p writer=%p xy=%d,%d style=%p scale=%p sample=%s",string,writer,x,y,style,scale,sample);
    }
    in_text=bracket;
    if(glyphs) string=pg_render_safe_text(string,owner,writer);
    if(glyphs) pg_render_text_begin(string,writer,owner);
    text_original(owner,string,writer,x,y,style,pen,scale);
    if(glyphs) pg_render_text_end();
    in_text=0;
}
static void __attribute__((thiscall)) quad(void *writer,float x0,float y0,float x1,float y1,float u0,float v0,float u1,float v1) {
    if(tracing && in_text && quad_notes++<6 && readable(writer,12)) {
        uintptr_t batch=*(uintptr_t *)writer;
        note("QUAD writer=%p batch=%p color=%08lX corners=%.3f,%.3f,%.3f,%.3f uv=%.3f,%.3f,%.3f,%.3f",
             writer,(void *)batch,(unsigned long)*(unsigned *)((char *)writer+8),x0,y0,x1,y1,u0,v0,u1,v1);
    }
    if(glyphs && pg_render_quad(writer,x0,y0,x1,y1)) { x1=x0; y1=y0; }
    quad_original(writer,x0,y0,x1,y1,u0,v0,u1,v1);
}
static void __attribute__((thiscall)) draw(void *context,unsigned type,int count,int start) {
    if(tracing && draw_notes++<5 && readable(context,0x1f4))
        note("DRAW context=%p type=%u count=%d start=%d device=%p array=%p",context,type,count,start,
             *(void **)((char *)context+0x144),*(void **)((char *)context+0x1f0));
    if(glyphs) { pg_render_ready(context); pg_render_batch(context,type,count,start); }
    draw_original(context,type,count,start);
    if(glyphs) pg_render_batch_end();
}
static void __attribute__((thiscall)) update(void *context,unsigned start,int count) {
    update_original(context,start,count);
    if(glyphs) pg_render_submit(context);
}
int mc_prompts_enabled(void) { return armed && dynamic; }
int mc_prompts_controller_source(unsigned player) { return player<MC_PLAYERS && sources[player].source==PG_CONTROLLER; }
const char *mc_prompts_menu_binding(unsigned code,McType type,unsigned player) {
    McControl control;
    if(!mc_prompts_enabled() || player>=MC_PLAYERS || !mc_prompts_controller_source(0) ||
       !pg_native_control(code,type,&control)) return NULL;
    /* Advanced's observed 006A4C4C PC text writer is incompatible with inline
       sprites. Return this cell's real binding in the selected glyph family. */
    return pg_control_name(control,pg_effective_style(styles[player],type));
}
static void source_changed(unsigned player) {
    if(tracing) note("SOURCE player=%u LastInputSource=%s",player+1,sources[player].source==PG_CONTROLLER?"Controller":"KeyboardMouse");
}
void mc_prompts_state(int role,int slot,const void *state,unsigned size) {
    unsigned p,i,now=GetTickCount();
    if(!mc_prompts_enabled() || !state) return;
    for(p=0;p<4;p++) { McType type; pg_input_connection(&sources[p],connected_slot(p,&type)>=0); }
    if(role==0 && size==256) {
        const unsigned char *keys=state;
        for(i=0;i<256;i++) if((keys[i]&128) && !(previous_keys[i]&128)) {
            unsigned owners=0,bank; uintptr_t *tables=(void *)(exe+0x668f40);
            /* Ownership is searched only on a real press, not each polling frame. */
            for(p=0;p<4;p++) for(bank=0;bank<4;bank++) {
                uintptr_t object=tables[bank*4+p]+0x18; unsigned row,s;
                if(!readable((void *)object,MC_ACTIONS*48)) continue;
                for(row=0;row<MC_ACTIONS;row++) for(s=0;s<4;s++) {
                    unsigned *b=(void *)(object+row*48+s*12);
                    if(b[1]==1 && b[2]==i) owners|=1u<<p;
                }
            }
            /* Shared menu keys are P1 in the original label path. No invented P2 owner. */
            for(p=0;p<4;p++) if((owners&(1u<<p)) && pg_input_event(&sources[p],PG_KEYBOARD_MOUSE,now)) source_changed(p);
        }
        memcpy(previous_keys,state,256);
    } else if(role==1 && size==sizeof(DIMOUSESTATE2)) {
        const DIMOUSESTATE2 *mouse=state; int edge=mouse->lZ!=0;
        for(i=0;i<8;i++) if((mouse->rgbButtons[i]&128) && !(previous_mouse[i]&128)) edge=1;
        if(pg_input_mouse(&sources[0],mouse->lX,mouse->lY,edge,now)) source_changed(0);
        memcpy(previous_mouse,mouse->rgbButtons,8);
    } else if(role==2 && slot>=0 && slot<MC_GAME_SLOTS && size==sizeof(DIJOYSTATE2)) {
        const DIJOYSTATE2 *pad=state,*prev=&previous_pads[slot]; int edge=0;
        for(i=0;i<128;i++) if((pad->rgbButtons[i]&128) && !(prev->rgbButtons[i]&128)) edge=1;
        for(i=0;i<4;i++) if(pad->rgdwPOV[i]!=prev->rgdwPOV[i] && (pad->rgdwPOV[i]&0xffff)!=0xffff) edge=1;
        if(previous_pad_valid[slot] && abs(pad->lZ-prev->lZ)>200) edge=1;
        for(p=0;p<4;p++) {
            McType type; int assigned=connected_slot(p,&type);
            if(assigned==slot && pg_input_pad(&sources[p],pad->lX/1000.f,pad->lY/1000.f,
               type==MC_PLAYSTATION?0:pad->lRx/1000.f,type==MC_PLAYSTATION?0:pad->lRy/1000.f,edge,deadzones[p],now)) source_changed(p);
        }
        previous_pads[slot]=*pad;
        previous_pad_valid[slot]=1;
    }
}
void mc_prompts_prepare(uintptr_t image,const WCHAR *ini,const McCore *core) {
    static const unsigned char lp[]={0xa1,0xfc,0xab,0xa6,0x00,0x83,0xec,0x08,0x85,0xc0,0x75,0x09};
    static const unsigned char rp[]={0x8b,0x54,0x24,0x08,0x85,0xd2,0x7c,0x30,0x56,0x8b,0x74,0x24,0x08};
    static const unsigned char tp[]={0x83,0xec,0x2c,0x8b,0x44,0x24,0x30,0x53,0x55,0x56,0x8b,0xf1};
    static const unsigned char qp[]={0x83,0xec,0x14,0x8b,0x44,0x24,0x28,0x8b,0x54,0x24,0x18};
    static const unsigned char dp[]={0x83,0xec,0x08,0x55,0x8b,0x6c,0x24,0x14,0x56,0x8b,0xf1,0x85,0xed};
    static const unsigned char up[]={0x83,0xec,0x10,0x56,0x8b,0xf1,0x57,0x33,0xff,0x39,0xbe,0x60,0x01,0x00,0x00};
    HMODULE gfx=GetModuleHandleW(L"libIGGfx.dll"); unsigned p; exe=image; controller_core=core;
    tracing=GetPrivateProfileIntW(L"General",L"PromptTrace",0,ini)==1;
    dynamic=GetPrivateProfileIntW(L"General",L"DynamicPrompts",0,ini)==1;
    glyphs=dynamic && GetPrivateProfileIntW(L"General",L"ControllerGlyphs",0,ini)==1;
    tutorial_controller_text=GetPrivateProfileIntW(L"General",L"TutorialControllerText",1,ini)==1;
    if(!tracing && !dynamic) return;
    for(p=0;p<4;p++) {
        WCHAR section[16],style[32],deadzone[32],legacy_deadzone[32];
        swprintf(section,16,L"Player%u",p+1);
        GetPrivateProfileStringW(section,L"GlyphStyle",L"Auto",style,32,ini); styles[p]=pg_style_parse(style);
        GetPrivateProfileStringW(L"Controller",L"Deadzone",L"0.15",legacy_deadzone,32,ini);
        GetPrivateProfileStringW(section,L"Deadzone",p?L"0.15":legacy_deadzone,deadzone,32,ini);
        deadzones[p]=pg_deadzone_parse(deadzone); pg_input_init(&sources[p]);
        note("CONFIG player=%u GlyphStyle=%s LastInputSource=KeyboardMouse",p+1,styles[p]==PG_XBOX?"Xbox":styles[p]==PG_PLAYSTATION?"PlayStation":"Auto");
    }
    if(exe!=0x400000u || !gfx || !gfx_layout((uintptr_t)gfx)) { note("REFUSED unsupported prompt image/layout"); return; }
    /* Validate the complete group before installing any entry interception. */
    if(!unique_at(exe,exe+0x219e30,lp,sizeof lp) || !unique_at(exe,exe+0x2294b0,rp,sizeof rp) ||
       !unique_at(exe,exe+0x1ee780,tp,sizeof tp) || !unique_at(exe,exe+0x1ee400,qp,sizeof qp) ||
       !unique_at((uintptr_t)gfx,(uintptr_t)gfx+0x352d0,dp,sizeof dp) ||
       !unique_at((uintptr_t)gfx,(uintptr_t)gfx+0x34e60,up,sizeof up)) { note("REFUSED nonunique/changed prompt signatures"); return; }
    label_original=(LabelFn)detour(exe,exe+0x219e30,lp,sizeof lp,8,(void *)label);
    read_original=(ReadFn)detour(exe,exe+0x2294b0,rp,sizeof rp,6,(void *)binding);
    text_original=(TextFn)detour(exe,exe+0x1ee780,tp,sizeof tp,7,(void *)text);
    quad_original=(QuadFn)detour(exe,exe+0x1ee400,qp,sizeof qp,7,(void *)quad);
    draw_original=(DrawFn)detour((uintptr_t)gfx,(uintptr_t)gfx+0x352d0,dp,sizeof dp,8,(void *)draw);
    update_original=(UpdateFn)detour((uintptr_t)gfx,(uintptr_t)gfx+0x34e60,up,sizeof up,6,(void *)update);
    armed=label_original && read_original && text_original && quad_original && draw_original && update_original;
    if(!armed) { int restored; dynamic=0; glyphs=0; restored=rollback_hooks(); note("REFUSED incomplete prompt hook transaction; restoration_verified=%d",restored); return; }
    note("TUTORIAL callsite=005EC061 mode=%s signature=%s",tutorial_controller_text?"controller text":"observe only",tutorial_hook()?"PASS":"REFUSED");
    note("ARMED label=00619E30 reader=006294B0 text=005EE780 quad=005EE400 draw=%p update=%p DynamicPrompts=%d ControllerGlyphs=%d complete=%d",
         (void *)((uintptr_t)gfx+0x352d0),(void *)((uintptr_t)gfx+0x34e60),dynamic,glyphs,armed);
}

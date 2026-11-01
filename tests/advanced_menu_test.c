/* Owned x86 fixture: no retail binary, UI asset or input injection. */
#include "../src/advanced_menu.c"
#undef NDEBUG
#include <assert.h>
void analog_log_event(const char *line) { fputs(line,stdout); }
int pg_render_active(void) { return 0; }
int mc_prompts_controller_source(unsigned player) { (void)player; return 0; }
const char *mc_prompts_menu_binding(unsigned code,McType type,unsigned player) { (void)code; (void)type; (void)player; return NULL; }
static void *expected_object;
static unsigned calls;
static void __attribute__((thiscall)) tick_mock(void *object,float dt) { assert(object==expected_object && dt==0.125f); calls++; }
static void __attribute__((thiscall)) dispatch_mock(void *object,unsigned msg,uintptr_t wp,uintptr_t lp) {
    assert(object==expected_object && msg==0x101 && wp==0x12345678 && lp==0xfedcba98); calls++;
}
static void __attribute__((thiscall)) select_mock(void *object,int row,int col) { assert(object==expected_object && row==-1 && col==2); calls++; }
static void __attribute__((thiscall)) notify_mock(void *object,unsigned event) { assert(object==expected_object && event==5); calls++; }
static void __attribute__((thiscall)) body_mock(void *object) { assert(object==expected_object); calls++; }
static const char *__attribute__((thiscall)) name_mock(void *object,unsigned kind,unsigned code) {
    assert(object==expected_object && kind==3 && code==21); calls++; return "original binding";
}
static uintptr_t esp(void) { uintptr_t value; __asm__ volatile("movl %%esp,%0":"=r"(value)); return value; }
static unsigned commands,last_protocol;
static uintptr_t fixture_root;
static void __attribute__((thiscall)) command_mock(void *object,unsigned msg,uintptr_t wp,uintptr_t lp) {
    assert((uintptr_t)object==fixture_root && msg==0x101 && lp==0);
    assert(wp==0x0d || wp==0x1b); commands++; last_protocol=(unsigned)wp;
}
static void retained_capture_and_edges(void) {
    unsigned char *owned=VirtualAlloc(NULL,0x66b000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    unsigned char *root=calloc(1,0x54),*list=calloc(1,0x6e0),*manager=calloc(1,0x12a10);
    McCore core; unsigned i; GUID unrelated={0}; DIJOYSTATE2 sample;
    assert(owned && root && list && manager);
    image=(uintptr_t)owned; active_list=(uintptr_t)list; fixture_root=(uintptr_t)root;
    *(uintptr_t *)(image+0x66ad34)=fixture_root;
    *(uintptr_t *)list=image+0x2a57ec; *(uintptr_t *)(list+0x2c)=fixture_root; list[0x30]=1;
    *(uintptr_t *)(list+0x54c)=(uintptr_t)manager;
    *(int *)(list+0x1c8)=-1; *(int *)(list+0x1cc)=-1;
    mc_core_init(&core); core.count=1; core.player[0].device_index=0;
    core.device[0].connected=1; core.device[0].detected_type=MC_XBOX; core.device[0].identity.instance.Data1=42;
    controller_core=&core; enabled=armed=1; tracing=0; pad_valid=0; entry_gate=1;
    dispatch_original=command_mock;
    memset(&sample,0,sizeof sample); for(i=0;i<4;i++) sample.rgdwPOV[i]=0xffffffff;
    sample.rgbButtons[0]=128;
    mc_advanced_state(2,&sample,sizeof sample,image+0x228835,&core.device[0].identity.instance);
    assert(entry_gate && !pending_accept);
    sample.rgbButtons[0]=0; sample.lX=300;
    mc_advanced_state(2,&sample,sizeof sample,image+0x228835,&core.device[0].identity.instance);
    assert(entry_gate);
    sample.lX=0;
    mc_advanced_state(2,&sample,sizeof sample,image+0x228835,&core.device[0].identity.instance);
    assert(!entry_gate);
    sample.rgbButtons[0]=128;
    mc_advanced_state(2,&sample,sizeof sample,image+0x228835,&core.device[0].identity.instance);
    assert(pending_accept); pending_accept=0; command(MENU_ACCEPT);
    assert(commands==1 && last_protocol==0x0d && consumed_accept);
    mc_advanced_state(2,&sample,sizeof sample,image+0x228835,&core.device[0].identity.instance);
    assert(!pending_accept);
    sample.rgbButtons[0]=0;
    mc_advanced_state(2,&sample,sizeof sample,image+0x228835,&core.device[0].identity.instance);
    assert(!consumed_accept);
    sample.rgbButtons[1]=128;
    mc_advanced_state(2,&sample,sizeof sample,image+0x228835,&core.device[0].identity.instance);
    assert(pending_back); pending_back=0; command(MENU_BACK);
    assert(commands==2 && last_protocol==0x1b);
    for(i=MENU_UP;i<=MENU_RIGHT;i++) command((MenuCommand)i);
    assert(commands==2);
    *(uintptr_t *)(image+0x66ad38)=1;
    command(MENU_ACCEPT); command(MENU_BACK); assert(commands==2);
    *(uintptr_t *)(image+0x66ad38)=0;
    *(int *)(list+0x1c8)=7; *(int *)(list+0x1cc)=1;
    sample.rgbButtons[1]=0; sample.rgbButtons[0]=128;
    mc_advanced_state(2,&sample,sizeof sample,image+0x228835,&core.device[0].identity.instance);
    command(MENU_ACCEPT); command(MENU_BACK);
    assert(commands==2 && !pending_accept && !pending_back && capturing());
    mc_advanced_lost(&core.device[0].identity.instance);
    assert(!pad_valid && entry_gate && !pending_accept && !pending_back);
    assert(word(active_list+0x1c8)==7 && word(active_list+0x1cc)==1);
    *(int *)(list+0x1c8)=-1; *(int *)(list+0x1cc)=-1;
    mc_advanced_state(2,&sample,sizeof sample,image+0x228835,&unrelated); assert(!pad_valid);
    core.device[0].connected=0;
    mc_advanced_state(2,&sample,sizeof sample,image+0x228835,&core.device[0].identity.instance); assert(!pad_valid);
    enabled=armed=0; active_list=0; image=0;
    free(root); free(list); free(manager); VirtualFree(owned,0,MEM_RELEASE);
    puts("PASS: contextual Accept/Back, entry neutral gate, held-button edges, original directions/modal/capture retained, foreign/disconnected pad refusal");
}
int main(void) {
    unsigned char object[0x6e0]={0}; uintptr_t before; unsigned i;
    McCore core; unsigned char pad[sizeof(DIJOYSTATE2)]={0};
    mc_core_init(&core); controller_core=&core; expected_object=object;
    tick_original=tick_mock; dispatch_original=dispatch_mock;
    select_original=select_mock; notify_original=notify_mock;
    draw_original=body_mock; name_original=name_mock;
    image=0; active_list=0; armed=enabled=0; tracing=0;
    before=esp();
    for(i=0;i<100;i++) {
        tick(object,0.125f); dispatch(object,0x101,0x12345678,0xfedcba98);
        select_cell(object,-1,2); notify_widget(object,5);
        list_draw(object); assert(!strcmp(binding_name(object,3,21),"original binding"));
        mc_advanced_state(2,pad,sizeof pad,0,NULL);
    }
    assert(calls==600 && esp()==before);
    assert(!armed && !notes && !hook_count);
    puts("PASS: six Advanced x86 thiscall object/arguments/returns/ESP preserved; disabled module leaves state untouched");
    retained_capture_and_edges();
    return 0;
}

/* Native x86 ABI fixture. Only owned memory and strings, no game files. */
#include "../src/controller_prompts.c"
#undef NDEBUG
#include <assert.h>
static const char pc_text[128]="PC hint with mouse";
static const char asset_text[128]="EXAMPLE:\\nPress Left = A\\nPress Right = B\\nPress Down = C\\nPress Up = D";
static const char test_key[128]="switching_hint";
static const char other_key[128]="other_hint";
static const char map_key[128]="automap_hint",potion_key[128]="potion_hint";
static const char map_text[128]="Click the right analog stick to cycle map sizes.";
static const char potion_text[128]="Press $ALLY to replenish your Health with Health Packs.";
static const char *fixture_asset=asset_text;
static void *observed_reader;
void analog_log_event(const char *s) { fputs(s,stdout); }
static const char *__attribute__((cdecl)) fake_localize(const char *key) {
    assert(key==test_key || key==other_key || key==map_key || key==potion_key); return pc_text;
}
static const char *__attribute__((thiscall)) fake_field(void *reader,const char *key,const char *empty) {
    assert(!strcmp(key,"text") && !*empty);
    observed_reader=reader;
    assert(*(uintptr_t *)((char *)reader+8)==0x12345678u);
    *(uintptr_t *)((char *)reader+8)=0x99999999u; return fixture_asset;
}
static const char *__attribute__((naked,cdecl)) call_site(const char *key __attribute__((unused))) {
    __asm__("subl $0x20, %esp\n\t"
            "movl $0x12345678, 0x1c(%esp)\n\t"
            "pushl 0x24(%esp)\n\tcall _tutorial_gate\n\taddl $4, %esp\n\t"
            "cmpl $0x12345678, 0x1c(%esp)\n\tje 1f\n\tud2\n\t"
            "1: addl $0x20, %esp\n\tret");
}
static void jump(unsigned offset,void *target) {
    unsigned char *at=(void *)(exe+offset); int32_t relative=(int32_t)((uintptr_t)target-(uintptr_t)at-5);
    at[0]=0xe9; memcpy(at+1,&relative,4);
}
int main(void) {
    DWORD old; unsigned char reader[12]={0};
    exe=(uintptr_t)VirtualAlloc(NULL,0x300000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE); assert(exe);
    memcpy((void *)(exe+0x285cbc),"text",5);
    jump(0x229bf0,(void *)fake_localize); jump(0x164b70,(void *)fake_field);
    assert(VirtualProtect((void *)exe,0x300000,PAGE_EXECUTE_READWRITE,&old));
    *(uintptr_t *)(reader+8)=0x12345678u;
    dynamic=1; tutorial_controller_text=1;
    assert(mc_tutorial_observe(test_key,reader)==pc_text);
    assert(*(uintptr_t *)(reader+8)==0x12345678u);
    sources[0].source=PG_CONTROLLER;
    assert(call_site(test_key)==pc_text); /* Disconnect alone cannot select prose. */
    sources[0].connected=1;
    assert(!strcmp(call_site(test_key),"EXAMPLE:\\nPress $DPAD_LF = A\\nPress $DPAD_RT = B\\nPress $DPAD_DN = C\\nPress $DPAD_UP = D") && observed_reader);
    assert(call_site(other_key)==asset_text); /* Other help fields stay original. */
    fixture_asset=map_text;
    assert(!strcmp(call_site(map_key),"Press $MAPTOGGLE to cycle map sizes."));
    fixture_asset=potion_text;
    assert(!strcmp(call_site(potion_key),"Press $TARGET_LOCK to replenish your Health with Health Packs."));
    fixture_asset=asset_text;
    tutorial_controller_text=0;
    assert(call_site(test_key)==pc_text);
    tutorial_controller_text=1; dynamic=0;
    assert(call_site(test_key)==pc_text);
    VirtualFree((void *)exe,0,MEM_RELEASE);
    puts("PASS: x86 scoped tutorial gate; caller ESP/RET and reader offset; native cdecl/thiscall; reader restored; keyboard/disabled original text");
    return 0;
}

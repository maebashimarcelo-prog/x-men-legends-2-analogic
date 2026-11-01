#include "prompt_model.h"
#undef NDEBUG
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    McControl c; McBinding row[4]={{1,28},{3,0x15},{0,0},{0,0}},b; PgInput p1,p2;
    {
        char converted[256],small[8];
        const char *help="EXAMPLE:\\nPress Left = hero A\\nPress Right = hero B\\nLeft alone remains prose.";
        assert(pg_help_tokens("switching_hint",help,converted,sizeof converted));
        assert(!strcmp(converted,"EXAMPLE:\\nPress $DPAD_LF = hero A\\nPress $DPAD_RT = hero B\\nLeft alone remains prose."));
        assert(!pg_help_tokens("other_hint",help,converted,sizeof converted));
        assert(!pg_help_tokens("switching_hint","Press Left and Right in prose",converted,sizeof converted));
        assert(!pg_help_tokens("switching_hint",help,small,sizeof small));
        assert(!pg_help_tokens("switching_hint","EXAMPLE:\\nPress Left = hero",converted,sizeof converted));
        assert(pg_help_tokens("switching_hint","EXAMPLE:\\nPress Left = one\\nPress Right = two\\nPress Down = three\\nPress Up = four",converted,sizeof converted));
        assert(!strcmp(converted,"EXAMPLE:\\nPress $DPAD_LF = one\\nPress $DPAD_RT = two\\nPress $DPAD_DN = three\\nPress $DPAD_UP = four"));
        assert(!pg_help_tokens("switching_hint","EXAMPLE:\\nPress Left = one\\nPress Right = two\\nPress Up = one\\nPress Up = two",converted,sizeof converted));
        assert(!pg_help_tokens("switching_hint","EXAMPLE:\\nPress Down = one\\nPress Up = two",converted,sizeof converted));
        assert(pg_help_tokens("automap_hint","Click the right analog stick to cycle map sizes.",converted,sizeof converted));
        assert(!strcmp(converted,"Press $MAPTOGGLE to cycle map sizes."));
        assert(pg_help_tokens("automap_hint","Click the right thumbstick to cycle map sizes.",converted,sizeof converted));
        assert(!pg_help_tokens("other_hint","Click the right thumbstick to cycle map sizes.",converted,sizeof converted));
        assert(!pg_help_tokens("automap_hint","The right thumbstick moves the camera.",converted,sizeof converted));
        assert(pg_help_tokens("potion_hint","Press $ALLY to replenish your Health with Health Packs.",converted,sizeof converted));
        assert(!strcmp(converted,"Press $TARGET_LOCK to replenish your Health with Health Packs."));
        assert(!pg_help_tokens("power_hint","Press $ALLY to replenish your Energy with Energy Packs.",converted,sizeof converted));
        assert(!pg_help_tokens("potion_hint","Press $TARGET_LOCK to replenish your Health with Health Packs.",converted,sizeof converted));
        /* The inserted tokens map to retail action 17/16 and live rows 14/15.
           Default and remapped rows resolve through the same binding policy. */
        McBinding directions[2][4]={{{1,0xcb},{3,0x12},{0,0},{0,0}},{{1,0xcd},{3,0x11},{0,0},{0,0}}};
        assert(pg_select_binding(directions[0],0,&b) && pg_native_control(b.code,MC_GENERIC,&c) && c==MC_DPAD_LEFT);
        assert(!strcmp(pg_control_name(c,PG_XBOX),"D-pad Left"));
        assert(!strcmp(pg_control_name(c,PG_PLAYSTATION),"D-pad Left"));
        assert(pg_select_binding(directions[1],0,&b) && pg_native_control(b.code,MC_GENERIC,&c) && c==MC_DPAD_RIGHT);
        directions[0][1].code=2; directions[1][1].code=0x18;
        assert(pg_select_binding(directions[0],0,&b) && pg_native_control(b.code,MC_XBOX,&c) && c==MC_LX_NEG);
        assert(pg_select_binding(directions[1],0,&b) && pg_native_control(b.code,MC_XBOX,&c) && c==MC_NORTH);
        assert(!strcmp(pg_control_name(c,PG_PLAYSTATION),"Triangle"));
        directions[0][1].code=999;
        assert(pg_select_binding(directions[0],0,&b) && !pg_native_control(b.code,MC_GENERIC,&c));
        assert(!pg_select_binding(directions[0],1,&b));
        McBinding map_row[4]={{1,0x32},{3,0x1e},{0,0},{0,0}};
        assert(pg_select_binding(map_row,0,&b) && pg_native_control(b.code,MC_GENERIC,&c) && c==MC_RIGHT_STICK);
        map_row[1].code=0x15;
        assert(pg_select_binding(map_row,0,&b) && pg_native_control(b.code,MC_GENERIC,&c) && c==MC_SOUTH);
    }
    for(unsigned i=0;i<MC_CONTROL_COUNT;i++) {
        assert(pg_control_name((McControl)i,PG_XBOX) && *pg_control_name((McControl)i,PG_XBOX));
        assert(pg_control_name((McControl)i,PG_PLAYSTATION) && *pg_control_name((McControl)i,PG_PLAYSTATION));
    }
    assert(!strcmp(pg_control_name(MC_SOUTH,PG_XBOX),"A"));
    assert(!strcmp(pg_control_name(MC_EAST,PG_XBOX),"B"));
    assert(!strcmp(pg_control_name(MC_WEST,PG_XBOX),"X"));
    assert(!strcmp(pg_control_name(MC_NORTH,PG_XBOX),"Y"));
    assert(!strcmp(pg_control_name(MC_SOUTH,PG_PLAYSTATION),"Cross"));
    assert(!strcmp(pg_control_name(MC_EAST,PG_PLAYSTATION),"Circle"));
    assert(!strcmp(pg_control_name(MC_WEST,PG_PLAYSTATION),"Square"));
    assert(!strcmp(pg_control_name(MC_NORTH,PG_PLAYSTATION),"Triangle"));
    assert(pg_effective_style(PG_AUTO,MC_GENERIC)==PG_XBOX);
    assert(pg_effective_style(PG_AUTO,MC_UNKNOWN)==PG_XBOX);
    assert(pg_effective_style(PG_AUTO,MC_PLAYSTATION)==PG_PLAYSTATION);
    assert(pg_effective_style(PG_XBOX,MC_PLAYSTATION)==PG_XBOX);
    assert(pg_effective_style(PG_PLAYSTATION,MC_GENERIC)==PG_PLAYSTATION);
    assert(pg_effective_style((PgStyle)999,MC_GENERIC)==PG_XBOX);
    assert(pg_style_parse(L"invalid")==PG_AUTO);
    assert(pg_style_parse(L"pLaYsTaTiOn")==PG_PLAYSTATION);
    assert(pg_deadzone_parse(L"0.25")==0.25f);
    assert(pg_deadzone_parse(L"invalid")==0.15f);
    assert(pg_deadzone_parse(L"NaN")==0.15f);
    assert(pg_deadzone_parse(L"  ")==0.15f);
    assert(pg_deadzone_parse(L"0.6")==0.15f);
    assert(pg_native_control(0x15,MC_GENERIC,&c) && c==MC_SOUTH);
    assert(pg_native_control(0x18,MC_XBOX,&c) && c==MC_NORTH);
    assert(!pg_native_control(999,MC_GENERIC,&c));
    assert(!pg_control_name((McControl)999,PG_XBOX));
    assert(pg_select_binding(row,0,&b) && b.code==0x15);
    row[1].code=0x18;
    assert(pg_select_binding(row,0,&b) && pg_native_control(b.code,MC_GENERIC,&c) && c==MC_NORTH);
    assert(!pg_select_binding(row,1,&b));
    assert(!pg_select_binding(row,-1,&b));
    assert(!pg_select_binding(row,10,&b));
    pg_input_init(&p1); pg_input_init(&p2);
    pg_input_connection(&p1,1); pg_input_connection(&p2,1);
    assert(p1.source==PG_KEYBOARD_MOUSE);
    assert(!pg_input_pad(&p1,0.1f,0.1f,0,0,0,0.15f,1000));
    assert(pg_input_pad(&p1,0.5f,0.5f,0,0,0,0.15f,1000));
    assert(p1.source==PG_CONTROLLER && p2.source==PG_KEYBOARD_MOUSE);
    assert(!pg_input_event(&p1,PG_KEYBOARD_MOUSE,1100));
    assert(pg_input_event(&p1,PG_KEYBOARD_MOUSE,1400));
    assert(!pg_input_pad(&p1,0.5f,0.5f,0,0,0,0.15f,1700)); /* Held stick cannot steal keyboard. */
    assert(!pg_input_pad(&p1,0,0,0,0,0,0.15f,1800));
    assert(pg_input_pad(&p1,1,0,0,0,0,0.15f,1900));
    for(unsigned t=2000;t<2600;t+=10) assert(!pg_input_mouse(&p1,1,0,0,t));
    assert(pg_input_mouse(&p1,9,0,0,2800));
    assert(pg_input_pad(&p1,0,0,0,0,1,0.15f,3100));
    pg_input_connection(&p1,0);
    assert(!pg_input_pad(&p1,1,0,0,0,1,0.15f,3400));
    assert(pg_input_event(&p1,PG_KEYBOARD_MOUSE,3400));
    pg_input_connection(&p1,1);
    assert(p1.source==PG_KEYBOARD_MOUSE);
    assert(pg_input_pad(&p1,0,0,0,0,1,0.15f,3800));
    assert(p2.source==PG_KEYBOARD_MOUSE);
    assert(pg_input_event(&p1,PG_KEYBOARD_MOUSE,4200));
    assert(pg_input_pad(&p1,0.5f,0.5f,0,0,0,0.15f,4600));
    assert(pg_input_event(&p1,PG_KEYBOARD_MOUSE,5000));
    assert(!pg_input_pad(&p1,0.51f,0.49f,0,0,0,0.15f,5400));
    assert(pg_input_pad(&p1,-0.5f,0.5f,0,0,0,0.15f,5500)); /* A new angle is meaningful even without recentering. */
    puts("PASS: 48 glyph names; style overrides/fallback; custom real binding; independent input sources; deadzone/hysteresis/debounce; mouse jitter; unplug/reconnect; invalid inputs");
    return 0;
}

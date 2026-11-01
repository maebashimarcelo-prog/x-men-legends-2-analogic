#include "controller_model.h"
#include <stdio.h>
#include <string.h>

#define REQUIRE(test) do { if (!(test)) { \
    fprintf(stderr, "FAIL line %u: %s\n", (unsigned)__LINE__, #test); return 1; } } while (0)

static McIdentity identity(unsigned instance, unsigned vid, unsigned pid, unsigned type) {
    McIdentity device = {0};
    device.instance.Data1 = instance; device.product.Data1 = (pid << 16) | vid;
    device.vid = vid; device.pid = pid; device.native_type = type;
    strcpy(device.name, "Controller (XBOX 360 For Windows)");
    return device;
}

int main(void) {
    McCore core;
    McIdentity a = identity(1, 0x3537, 0x1040, 0x15);
    McIdentity b = identity(2, 0x045e, 0x028e, 0x15);
    McIdentity supplemental = identity(3, 0x045e, 0x028e, 0x1c);
    McIdentity sony = identity(4, 0x054c, 0x0ce6, 0x15);
    McBinding map[MC_ACTIONS][MC_BINDING_SLOTS] = {{{0}}};
    const McPresetAction *preset;
    McBindingWrite writes[MC_PRESET_WRITES];
    unsigned i, count, rows = 0;

    REQUIRE(mc_classify(&a) == MC_GENERIC); /* A brand-like name cannot prove type. */
    a.xinput_path = 1; REQUIRE(mc_classify(&a) == MC_XBOX);
    REQUIRE(mc_classify(&b) == MC_XBOX);
    REQUIRE(mc_classify(&supplemental) == MC_GENERIC);
    REQUIRE(mc_classify(&sony) == MC_PLAYSTATION);
    sony.pid = 0xffff; REQUIRE(mc_classify(&sony) == MC_GENERIC);

    mc_core_init(&core);
    core.player[1].controller_index = 0; /* Conflicting requests never duplicate ownership. */
    REQUIRE(mc_device_observe(&core, &a, 0) == 0);
    REQUIRE(mc_device_observe(&core, &supplemental, 1) == 1);
    mc_assign_players(&core);
    REQUIRE(core.player[0].device_index == 0 && core.player[1].device_index == -1);
    REQUIRE(core.player[2].device_index == -1 && core.player[3].device_index == -1);

    mc_begin_inventory(&core); /* A disappears; B becomes available at a different slot. */
    REQUIRE(mc_device_observe(&core, &b, 3) == 2);
    mc_assign_players(&core);
    REQUIRE(core.player[0].device_index == 0 && !core.device[0].connected);
    REQUIRE(core.player[3].device_index == 2 && core.device[2].connected);
    mc_begin_inventory(&core);
    REQUIRE(mc_device_observe(&core, &b, 3) == 2); /* Enumeration order changes. */
    REQUIRE(mc_device_observe(&core, &a, 0) == 0);
    mc_assign_players(&core);
    REQUIRE(core.count == 3 && core.device[0].player == 0 && core.device[2].player == 3);
    REQUIRE(core.player[0].reserved_instance.Data1 == 1);
    core.player[0].type_override = MC_PLAYSTATION;
    REQUIRE(core.device[0].detected_type == MC_XBOX && core.device[0].identity.vid == 0x3537);

    REQUIRE(mc_can_install_preset(map, 0));
    map[4][0] = (McBinding){3, 0x17};
    REQUIRE(!mc_can_install_preset(map, 0) && mc_can_install_preset(map, 1));
    map[19][1] = (McBinding){1, 0x48};
    REQUIRE(!mc_can_install_preset(map, 1)); /* Even restore preserves alternate keyboard/mouse. */
    map[19][1].kind = 2; REQUIRE(!mc_can_install_preset(map, 1));

    preset = mc_console_preset(&count);
    REQUIRE(count == 23);
    for (i = 0; i < count; i++) {
        REQUIRE(preset[i].action < 32 && !(rows & (1u << preset[i].action)));
        rows |= 1u << preset[i].action;
        REQUIRE(mc_native_code(preset[i].control, MC_GENERIC) != 0);
        REQUIRE(mc_native_code(preset[i].control, MC_PLAYSTATION) == 0);
        for(unsigned j=0;j<i;j++) REQUIRE(preset[j].control!=preset[i].control);
        if(preset[i].action==9) REQUIRE(preset[i].control==MC_LEFT_SHOULDER);
        if(preset[i].action==10) REQUIRE(preset[i].control==MC_RIGHT_SHOULDER);
        if(preset[i].action==11) REQUIRE(preset[i].control==MC_LEFT_TRIGGER);
    }
    REQUIRE(mc_native_code(MC_SOUTH, MC_XBOX) == 0x15);
    REQUIRE(mc_native_code(MC_LY_NEG, MC_XBOX) == 4);
    REQUIRE(mc_native_code(MC_DPAD_UP, MC_XBOX) == 0x14);
    REQUIRE(rows & (1u << 11));
    REQUIRE(mc_native_code(MC_LEFT_SHOULDER,MC_XBOX)==0x19);
    /* A restore clears every stale pad binding before publication, while
       retaining keyboard and mouse rows. This avoids duplicate actions. */
    memset(map,0,sizeof map);
    map[7][0]=(McBinding){3,0x15};
    map[41][3]=(McBinding){4,0x16};
    map[0][0]=(McBinding){1,0x11}; map[19][2]=(McBinding){2,5};
    REQUIRE(mc_plan_preset(map,MC_GENERIC,0,0,writes)==0);
    count=mc_plan_preset(map,MC_GENERIC,0,1,writes);
    REQUIRE(count==25);
    for (i=0;i<count;i++) map[writes[i].action][writes[i].slot]=(McBinding){writes[i].kind,writes[i].code};
    REQUIRE(map[7][0].kind==0 && map[41][3].kind==0);
    REQUIRE(map[0][0].kind==1 && map[0][0].code==0x11);
    REQUIRE(map[19][2].kind==2 && map[19][2].code==5);
    REQUIRE(map[4][1].kind==3 && map[4][1].code==0x15);
    REQUIRE(map[7][1].kind==3 && map[7][1].code==0x17);
    REQUIRE(map[9][1].code==0x19 && map[10][1].code==0x1a && map[11][1].code==5);
    REQUIRE(mc_plan_preset(map,MC_PLAYSTATION,0,1,writes)==0);
    REQUIRE(mc_plan_preset(map,MC_GENERIC,10,1,writes)==0);
    memset(map,0,sizeof map);
    REQUIRE(mc_plan_preset(map,MC_GENERIC,0,0,writes)==23);
    map[9][1]=(McBinding){3,5}; /* Existing LT recovery mapping remains custom. */
    map[10][1]=(McBinding){3,0x18};
    {
        McBinding original[MC_ACTIONS][MC_BINDING_SLOTS];
        memcpy(original,map,sizeof map);
        REQUIRE(mc_plan_preset(map,MC_GENERIC,0,0,writes)==0);
        REQUIRE(!memcmp(original,map,sizeof map));
    }
    puts("PASS: identity, conservative classification, reservations, unique player ownership and custom-binding protection.");
    puts("PASS: fresh ConsoleDefault and explicit restore remove stale pad actions, preserving keyboard/mouse bindings.");
    return 0;
}

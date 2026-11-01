/* Authored device model and logical console actions. No game memory access. */
#include "controller_model.h"
#include <string.h>

static int listed(unsigned value, const unsigned *list, unsigned count) {
    unsigned i;
    for (i = 0; i < count; i++) if (value == list[i]) return 1;
    return 0;
}

int mc_primary_device(const McIdentity *identity) {
    unsigned type = identity->native_type & 255u;
    /* DI8 GAMEPAD / JOYSTICK. Supplemental interfaces remain available,
       but cannot be automatically assigned as additional physical pads. */
    return type == 0x15 || type == 0x14;
}

McType mc_classify(const McIdentity *identity) {
    static const unsigned xbox[] = {0x028e,0x028f,0x02d1,0x02dd,0x02ea,0x02fd,
                                    0x0b00,0x0b12,0x0b13,0x0b20,0x0b21,0x0b22};
    static const unsigned sony[] = {0x0268,0x05c4,0x09cc,0x0ba0,0x0ce6,0x0df2};
    if (!mc_primary_device(identity)) return MC_GENERIC;
    if (identity->xinput_path || (identity->vid == 0x045e &&
        listed(identity->pid, xbox, sizeof xbox / sizeof xbox[0]))) return MC_XBOX;
    if (identity->vid == 0x054c && listed(identity->pid, sony, sizeof sony / sizeof sony[0]))
        return MC_PLAYSTATION;
    return MC_GENERIC;
}

const char *mc_type_name(McType type) {
    switch (type) {
        case MC_XBOX: return "Xbox";
        case MC_PLAYSTATION: return "PlayStation";
        case MC_GENERIC: return "Generic";
        default: return "Unknown";
    }
}

void mc_core_init(McCore *core) {
    unsigned i;
    memset(core, 0, sizeof *core);
    for (i = 0; i < MC_PLAYERS; i++) {
        core->player[i].controller_index = (int)i;
        core->player[i].device_index = -1;
        core->player[i].console_default = 1;
    }
}

void mc_begin_inventory(McCore *core) {
    unsigned i;
    for (i = 0; i < core->count; i++) core->device[i].connected = 0;
}

int mc_device_observe(McCore *core, const McIdentity *identity, int game_slot) {
    unsigned i;
    for (i = 0; i < core->count; i++)
        if (!memcmp(&core->device[i].identity.instance, &identity->instance, sizeof(GUID))) break;
    if (i == core->count) {
        if (i == MC_DEVICES) return -1;
        core->count++;
        core->device[i].game_slot = -1;
        core->device[i].player = -1;
    }
    core->device[i].identity = *identity;
    core->device[i].detected_type = mc_classify(identity);
    core->device[i].connected = 1;
    if (game_slot >= 0 && game_slot < MC_GAME_SLOTS) core->device[i].game_slot = game_slot;
    return (int)i;
}

void mc_assign_players(McCore *core) {
    const GUID empty = {0};
    unsigned player, i;
    for (i = 0; i < core->count; i++) core->device[i].player = -1;
    for (player = 0; player < MC_PLAYERS; player++) {
        McPlayer *p = &core->player[player];
        p->device_index = -1;
        if (p->controller_index < 0 || p->controller_index >= MC_GAME_SLOTS) continue;
        for (i = 0; i < core->count; i++) {
            McDevice *device = &core->device[i];
            if (device->game_slot != p->controller_index || device->player >= 0 ||
                !mc_primary_device(&device->identity)) continue;
            /* Keep the logical reservation even while this GUID is absent. */
            if (memcmp(&p->reserved_instance, &empty, sizeof empty) &&
                memcmp(&p->reserved_instance, &device->identity.instance, sizeof empty)) continue;
            p->reserved_instance = device->identity.instance;
            p->device_index = (int)i;
            device->player = (int)player;
            break;
        }
    }
}

/* Retail row keys and igct.bnx distinguish Ally (Energy Pack), TargetLock
   (Health Pack) and Solo (Call Allies). LT retains the documented console
   Call Allies position; LB/RB form the recovery pair. */
static const McPresetAction console_default[] = {
    {0, MC_LY_NEG}, {1, MC_LY_POS}, {2, MC_LX_NEG}, {3, MC_LX_POS},
    {4, MC_SOUTH}, {5, MC_EAST}, {6, MC_NORTH}, {7, MC_WEST},
    {8, MC_RIGHT_TRIGGER}, {9, MC_LEFT_SHOULDER}, {10, MC_RIGHT_SHOULDER},
    {11, MC_LEFT_TRIGGER},
    {12, MC_DPAD_UP}, {13, MC_DPAD_DOWN}, {14, MC_DPAD_LEFT}, {15, MC_DPAD_RIGHT},
    {16, MC_RIGHT_STICK}, {17, MC_START}, {18, MC_BACK},
    {19, MC_RY_NEG}, {20, MC_RY_POS}, {21, MC_RX_NEG}, {22, MC_RX_POS}
};

const McPresetAction *mc_console_preset(unsigned *count) {
    *count = sizeof console_default / sizeof console_default[0];
    return console_default;
}

unsigned mc_native_code(McControl control, McType type) {
    /* Xbox / Generic compatible DI layout: LX/LY, combined trigger Z, Rx/Ry.
       Native Sony HID indices differ. Classification alone is insufficient
       to install a Sony preset safely; an adapter requires a hardware map. */
    if (type == MC_PLAYSTATION) return 0;
    switch (control) {
        case MC_LX_NEG: return 2; case MC_LX_POS: return 1;
        case MC_LY_NEG: return 4; case MC_LY_POS: return 3;
        case MC_SOUTH: return 0x15; case MC_EAST: return 0x16;
        case MC_WEST: return 0x17; case MC_NORTH: return 0x18;
        case MC_LEFT_TRIGGER: return 5; case MC_RIGHT_TRIGGER: return 6;
        case MC_LEFT_SHOULDER: return 0x19;
        case MC_RIGHT_SHOULDER: return 0x1a;
        case MC_DPAD_UP: return 0x14; case MC_DPAD_DOWN: return 0x13;
        case MC_DPAD_LEFT: return 0x12; case MC_DPAD_RIGHT: return 0x11;
        case MC_RIGHT_STICK: return 0x1e; case MC_START: return 0x1c;
        case MC_BACK: return 0x1b;
        case MC_RY_NEG: return 10; case MC_RY_POS: return 9;
        case MC_RX_NEG: return 8; case MC_RX_POS: return 7;
        default: return 0;
    }
}

int mc_has_pad_bindings(const McBinding map[MC_ACTIONS][MC_BINDING_SLOTS]) {
    unsigned action, slot;
    for (action = 0; action < MC_ACTIONS; action++) for (slot = 0; slot < MC_BINDING_SLOTS; slot++)
        if (map[action][slot].kind >= 3 && map[action][slot].kind <= 12 &&
            map[action][slot].code != 0 && map[action][slot].code != 0xffffffffu) return 1;
    return 0;
}

int mc_can_install_preset(const McBinding map[MC_ACTIONS][MC_BINDING_SLOTS], int restore) {
    unsigned action;
    if (!restore && mc_has_pad_bindings(map)) return 0;
    for (action = 0; action < MC_ACTIONS; action++) {
        unsigned kind = map[action][1].kind;
        /* Always preserve alternate keyboard/mouse mappings. */
        if (kind == 1 || kind == 2) return 0;
    }
    return 1;
}

unsigned mc_plan_preset(const McBinding map[MC_ACTIONS][MC_BINDING_SLOTS],
                        McType type, int game_slot, int restore,
                        McBindingWrite writes[MC_PRESET_WRITES]) {
    unsigned action,slot,count=0,i;
    if (type==MC_PLAYSTATION || game_slot<0 || game_slot>=MC_GAME_SLOTS ||
        !mc_can_install_preset(map,restore)) return 0;
    if (restore) for (action=0;action<MC_ACTIONS;action++) for (slot=0;slot<MC_BINDING_SLOTS;slot++) {
        unsigned kind=map[action][slot].kind;
        if (kind>=3 && kind<=12)
            writes[count++]=(McBindingWrite){action,slot,0,0};
    }
    for (i=0;i<sizeof console_default/sizeof console_default[0];i++) {
        const McPresetAction *item=&console_default[i];
        writes[count++]=(McBindingWrite){item->action,1,3u+(unsigned)game_slot,
                                        mc_native_code(item->control,type)};
    }
    return count;
}

#ifndef XML2_CONTROLLER_MODEL_H
#define XML2_CONTROLLER_MODEL_H

#include "xml2_analog.h"

#define MC_PLAYERS 4
#define MC_DEVICES 32
#define MC_GAME_SLOTS 10
#define MC_ACTIONS 42
#define MC_BINDING_SLOTS 4

typedef enum { MC_UNKNOWN, MC_XBOX, MC_PLAYSTATION, MC_GENERIC } McType;
typedef struct {
    GUID instance, product;
    char name[260];
    unsigned vid, pid, native_type;
    int xinput_path;
} McIdentity;
typedef struct {
    McIdentity identity;
    McType detected_type;
    int connected, game_slot, player;
} McDevice;
typedef struct {
    int controller_index;
    McType type_override;
    int console_default, restore_defaults;
    int device_index;
    GUID reserved_instance;
} McPlayer;
typedef struct {
    McDevice device[MC_DEVICES];
    unsigned count;
    McPlayer player[MC_PLAYERS];
} McCore;

typedef enum {
    MC_LX_NEG, MC_LX_POS, MC_LY_NEG, MC_LY_POS,
    MC_SOUTH, MC_EAST, MC_WEST, MC_NORTH,
    MC_LEFT_TRIGGER, MC_RIGHT_TRIGGER, MC_RIGHT_SHOULDER,
    MC_DPAD_UP, MC_DPAD_DOWN, MC_DPAD_LEFT, MC_DPAD_RIGHT,
    MC_RIGHT_STICK, MC_START, MC_BACK,
    MC_RY_NEG, MC_RY_POS, MC_RX_NEG, MC_RX_POS,
    MC_LEFT_SHOULDER, MC_LEFT_STICK, MC_CONTROL_COUNT
} McControl;
typedef struct { unsigned action; McControl control; } McPresetAction;
typedef struct { unsigned kind, code; } McBinding;
typedef struct { unsigned action, slot, kind, code; } McBindingWrite;
#define MC_PRESET_WRITES (MC_ACTIONS * MC_BINDING_SLOTS + 23)

McType mc_classify(const McIdentity *identity);
const char *mc_type_name(McType type);
void mc_core_init(McCore *core);
int mc_device_observe(McCore *core, const McIdentity *identity, int game_slot);
void mc_begin_inventory(McCore *core);
void mc_assign_players(McCore *core);
int mc_primary_device(const McIdentity *identity);
const McPresetAction *mc_console_preset(unsigned *count);
unsigned mc_native_code(McControl control, McType type);
/* A mapping contains all 42 actions and all four original binding slots. */
int mc_has_pad_bindings(const McBinding map[MC_ACTIONS][MC_BINDING_SLOTS]);
int mc_can_install_preset(const McBinding map[MC_ACTIONS][MC_BINDING_SLOTS], int restore);
unsigned mc_plan_preset(const McBinding map[MC_ACTIONS][MC_BINDING_SLOTS],
                        McType type, int game_slot, int restore,
                        McBindingWrite writes[MC_PRESET_WRITES]);

#endif

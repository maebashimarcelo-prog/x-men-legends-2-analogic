#ifndef XML2_PROMPT_MODEL_H
#define XML2_PROMPT_MODEL_H
#include "controller_model.h"
typedef enum { PG_AUTO, PG_XBOX, PG_PLAYSTATION } PgStyle;
typedef enum { PG_KEYBOARD_MOUSE, PG_CONTROLLER } PgSource;
typedef struct {
    PgSource source;
    unsigned changed, mouse_accum, mouse_start;
    int connected, analog_armed;
    float anchor_x,anchor_y,anchor_rx,anchor_ry;
} PgInput;
PgStyle pg_style_parse(const WCHAR *value);
float pg_deadzone_parse(const WCHAR *value);
PgStyle pg_effective_style(PgStyle override, McType type);
const char *pg_control_name(McControl button,PgStyle style);
int pg_native_control(unsigned code,McType type,McControl *control);
int pg_select_binding(const McBinding row[MC_BINDING_SLOTS],int game_slot,McBinding *binding);
int pg_help_tokens(const char *key,const char *text,char *output,size_t capacity);
void pg_input_init(PgInput *state);
int pg_input_event(PgInput *state,PgSource source,unsigned now);
int pg_input_pad(PgInput *state,float x,float y,float rx,float ry,int edge,float deadzone,unsigned now);
int pg_input_mouse(PgInput *state,int dx,int dy,int edge,unsigned now);
void pg_input_connection(PgInput *state,int connected);
#endif

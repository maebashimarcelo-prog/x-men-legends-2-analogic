/* Presentation policy only: these functions never write any input buffer. */
#include "prompt_model.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
float pg_deadzone_parse(const WCHAR *s) {
    WCHAR *end; double value; int parsed;
    if(!s) return 0.15f;
    value=wcstod(s,&end);
    parsed=end!=s;
    while(*end==' ' || *end=='\t') end++;
    return parsed && !*end && isfinite(value) && value>=0 && value<=0.5?(float)value:0.15f;
}
PgStyle pg_style_parse(const WCHAR *s) {
    if(s && !_wcsicmp(s,L"Xbox")) return PG_XBOX;
    if(s && !_wcsicmp(s,L"PlayStation")) return PG_PLAYSTATION;
    return PG_AUTO;
}
PgStyle pg_effective_style(PgStyle override,McType type) {
    if(override==PG_XBOX || override==PG_PLAYSTATION) return override;
    return type==MC_PLAYSTATION?PG_PLAYSTATION:PG_XBOX;
}
const char *pg_control_name(McControl c,PgStyle style) {
    static const char *xbox[]={"LS Left","LS Right","LS Up","LS Down","A","B","X","Y","LT","RT","RB",
        "D-pad Up","D-pad Down","D-pad Left","D-pad Right","RS","Menu","View","RS Up","RS Down","RS Left","RS Right","LB","LS"};
    static const char *ps[]={"L3 Left","L3 Right","L3 Up","L3 Down","Cross","Circle","Square","Triangle","L2","R2","R1",
        "D-pad Up","D-pad Down","D-pad Left","D-pad Right","R3","Options","Create","R3 Up","R3 Down","R3 Left","R3 Right","L1","L3"};
    return (unsigned)c<MC_CONTROL_COUNT?(style==PG_PLAYSTATION?ps[c]:xbox[c]):NULL;
}
int pg_native_control(unsigned code,McType type,McControl *out) {
    static const McControl axes[]={MC_LX_POS,MC_LX_NEG,MC_LY_POS,MC_LY_NEG,MC_LEFT_TRIGGER,MC_RIGHT_TRIGGER,MC_RX_POS,MC_RX_NEG,MC_RY_POS,MC_RY_NEG};
    static const McControl pov[]={MC_DPAD_RIGHT,MC_DPAD_LEFT,MC_DPAD_DOWN,MC_DPAD_UP};
    static const McControl xbox[]={MC_SOUTH,MC_EAST,MC_WEST,MC_NORTH,MC_LEFT_SHOULDER,MC_RIGHT_SHOULDER,MC_BACK,MC_START,MC_LEFT_STICK,MC_RIGHT_STICK};
    /* Native Sony HID button order differs from Xbox-compatible DirectInput.
       Axis layouts are not hardware-validated; do not claim an icon for them. */
    static const McControl sony[]={MC_WEST,MC_SOUTH,MC_EAST,MC_NORTH,MC_LEFT_SHOULDER,MC_RIGHT_SHOULDER,MC_LEFT_TRIGGER,MC_RIGHT_TRIGGER,MC_BACK,MC_START,MC_LEFT_STICK,MC_RIGHT_STICK};
    if(!out) return 0;
    if(code>=0x11 && code<=0x14) { *out=pov[code-0x11]; return 1; }
    if(code>=1 && code<=10 && type!=MC_PLAYSTATION) { *out=axes[code-1]; return 1; }
    if(code>=0x15) {
        unsigned index=code-0x15;
        if(type==MC_PLAYSTATION && index<sizeof sony/sizeof sony[0]) { *out=sony[index]; return 1; }
        if(type!=MC_PLAYSTATION && index<sizeof xbox/sizeof xbox[0]) { *out=xbox[index]; return 1; }
    }
    return 0;
}
int pg_select_binding(const McBinding row[MC_BINDING_SLOTS],int slot,McBinding *out) {
    unsigned i;
    if(!row || !out || slot<0 || slot>=MC_GAME_SLOTS) return 0;
    for(i=0;i<MC_BINDING_SLOTS;i++) if(row[i].kind==(unsigned)slot+3 && row[i].code && row[i].code!=~0u) { *out=row[i]; return 1; }
    return 0;
}
/* Scoped, verified binding instructions in three retail help fields. Native
   tokens resolve the current action/row/binding; no physical control is assumed. */
int pg_help_tokens(const char *key,const char *text,char *output,size_t capacity) {
    static const char *labels[]={"\\nPress Left = ","\\nPress Right = ","\\nPress Down = ","\\nPress Up = "};
    static const char *tokens[]={"\\nPress $DPAD_LF = ","\\nPress $DPAD_RT = ","\\nPress $DPAD_DN = ","\\nPress $DPAD_UP = "};
    static const char *map_labels[]={"Click the right analog stick to cycle", "Click the right thumbstick to cycle"};
    static const char health_label[]="Press $ALLY to replenish your Health with Health Packs.";
    size_t used=0; unsigned found=0; int switching,automap,potion;
    if(!key || !text || !output || !capacity) return 0;
    switching=!strcmp(key,"switching_hint") && strstr(text,"EXAMPLE:");
    automap=!strcmp(key,"automap_hint"); potion=!strcmp(key,"potion_hint");
    if(!switching && !automap && !potion) return 0;
    while(*text) {
        const char *copy=text; size_t take=1,advance=1; unsigned i;
        for(i=0;switching && i<4;i++) if(!strncmp(text,labels[i],strlen(labels[i]))) {
            if(found&(1u<<i)) return 0;
            found|=1u<<i; copy=tokens[i]; take=strlen(copy);
            advance=strlen(labels[i]); break;
        }
        for(i=0;automap && i<2;i++) if(!strncmp(text,map_labels[i],strlen(map_labels[i]))) {
            if(found) return 0;
            found=1; copy="Press $MAPTOGGLE to cycle"; take=strlen(copy); advance=strlen(map_labels[i]); break;
        }
        if(potion && !strncmp(text,health_label,sizeof health_label-1)) {
            if(found) return 0;
            found=1; copy="Press $TARGET_LOCK to replenish your Health with Health Packs.";
            take=strlen(copy); advance=sizeof health_label-1;
        }
        if(take>=capacity-used) return 0;
        memcpy(output+used,copy,take); used+=take; text+=advance;
    }
    output[used]=0;
    return switching?(found&3u)==3u:found==1;
}
void pg_input_init(PgInput *s) { memset(s,0,sizeof *s); }
int pg_input_event(PgInput *s,PgSource source,unsigned now) {
    if(source==PG_CONTROLLER && !s->connected) return 0;
    if(source==s->source || (s->changed && (unsigned)(now-s->changed)<250u)) return 0;
    s->source=source; s->changed=now; return 1;
}
int pg_input_pad(PgInput *s,float x,float y,float rx,float ry,int edge,float deadzone,unsigned now) {
    float enter=fmaxf(0.20f,deadzone+0.05f),leave=fmaxf(0.15f,deadzone);
    float magnitude=fmaxf(x*x+y*y,rx*rx+ry*ry);
    float dx=x-s->anchor_x,dy=y-s->anchor_y,drx=rx-s->anchor_rx,dry=ry-s->anchor_ry;
    int moved=fmaxf(dx*dx+dy*dy,drx*drx+dry*dry)>0.12f*0.12f;
    int significant=edge || (magnitude>enter*enter && (!s->analog_armed || moved));
    if(magnitude<=leave*leave) {
        s->analog_armed=0;
        s->anchor_x=x; s->anchor_y=y; s->anchor_rx=rx; s->anchor_ry=ry;
    }
    else if(magnitude>enter*enter) s->analog_armed=1;
    if(significant) { s->anchor_x=x; s->anchor_y=y; s->anchor_rx=rx; s->anchor_ry=ry; }
    return significant?pg_input_event(s,PG_CONTROLLER,now):0;
}
int pg_input_mouse(PgInput *s,int dx,int dy,int edge,unsigned now) {
    unsigned amount=(unsigned)(abs(dx)+abs(dy));
    if(!s->mouse_start || now-s->mouse_start>150) { s->mouse_start=now; s->mouse_accum=0; }
    if(amount>=2) s->mouse_accum+=amount; /* 1-count jitter never accumulates. */
    if(edge || s->mouse_accum>=8) { s->mouse_accum=0; return pg_input_event(s,PG_KEYBOARD_MOUSE,now); }
    return 0;
}
void pg_input_connection(PgInput *s,int connected) {
    s->connected=connected;
    if(!connected) s->analog_armed=0; /* A connection alone is not input. */
}

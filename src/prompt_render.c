/* Additional owned atlas at the existing D3D8 text batch; no renderer replacement. */
#define CINTERFACE
#define COBJMACROS
#include <d3d8.h>
#include "prompt_render.h"
#include "prompt_model.h"
#include "controller_atlas.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

typedef struct { uintptr_t array; unsigned vertex,atlas; float x0,y0,x1,y1; DWORD color; } GlyphQuad;
typedef struct { float x,y,z; DWORD color; float u,v; } Vertex;
#define QUEUE_MAX 256
static GlyphQuad queue[QUEUE_MAX];
static unsigned queued;
static IDirect3DDevice8 *device;
static IDirect3DTexture8 *texture;
static DWORD state_block;
static unsigned char code_to_atlas[256],atlas_to_code[48];
static int metrics_ready,failed;
static const WCHAR *cursor;
static unsigned cursor_index,expected,emitted;
static unsigned cursor_style;
static unsigned pending_inline=QUEUE_MAX;
static DWORD next_metrics_attempt;
static uintptr_t current_context;
static unsigned current_start,current_end;
static int current_valid;
static unsigned draw_count;
static unsigned sink_notes;
static uintptr_t font_table_address=0x8161f8u;
static _Thread_local WCHAR fallback_text[2048];
static void log_line(const char *s) { analog_log_event(s); }
static int readable(const void *p,size_t n) {
    MEMORY_BASIC_INFORMATION m; uintptr_t a=(uintptr_t)p;
    return p && VirtualQuery(p,&m,sizeof m)==sizeof m && m.State==MEM_COMMIT &&
        !(m.Protect&(PAGE_NOACCESS|PAGE_GUARD)) && n<=(uintptr_t)m.BaseAddress+m.RegionSize-a;
}
void pg_render_disable(const char *reason) {
    char line[240];
    if(!failed) { snprintf(line,sizeof line,"Glyph renderer DISABLED: %s; input features independent\r\n",reason); log_line(line); }
    failed=1; cursor=NULL; queued=0;
}
int pg_render_active(void) { return !failed && texture && metrics_ready; }
/* The game's unused 8-bit cells carry metrics only. No font pixels/UVs change. */
static int publish_metrics(void) {
    uintptr_t table=font_table_address; unsigned c,i,j,free_count=0; int caps[4]={0},baseline[4]={0};
    char line[200];
    if(!readable((void *)table,4+4*0x1c18u) || *(uintptr_t *)table!=0x69d69cu) return 0;
    memset(code_to_atlas,255,sizeof code_to_atlas);
    for(i=0;i<4;i++) {
        unsigned char *glyph=(void *)(table+i*0x1c18u+0x18);
        unsigned best_height=0,best_base=0;
        for(c='A';c<='Z';c++) {
            short *g=(void *)(glyph+c*28); unsigned hcount=0,bcount=0;
            int b; memcpy(&b,(char *)g+8,4);
            if(g[0]<=0 || g[1]<=0 || g[1]>128 || b<=0 || b>128) continue;
            for(j='A';j<='Z';j++) {
                short *other=(void *)(glyph+j*28); int ob; memcpy(&ob,(char *)other+8,4);
                if(other[0]>0 && other[1]>0) { if(other[1]==g[1]) hcount++; if(ob==b) bcount++; }
            }
            if(hcount>best_height) { caps[i]=g[1]; best_height=hcount; }
            if(bcount>best_base) { baseline[i]=b; best_base=bcount; }
        }
    }
    if(!caps[0] && !caps[1] && !caps[2] && !caps[3]) return 0;
    for(c=128;c<256 && free_count<26;c++) {
        int empty=1;
        for(i=0;i<4;i++) {
            short *g=(void *)(table+i*0x1c18u+0x18+c*28);
            if(g[0] || g[1] || g[2]) empty=0;
        }
        if(empty) { atlas_to_code[free_count]=(unsigned char)c; code_to_atlas[c]=(unsigned char)free_count++; }
    }
    if(free_count!=26) { pg_render_disable("fewer than 26 shared empty font cells"); return 0; }
    for(i=0;i<4;i++) if(caps[i] && baseline[i]) for(j=0;j<24;j++) {
        short *g=(void *)(table+i*0x1c18u+0x18+atlas_to_code[j]*28);
        int h=(caps[i]*3+1)/2,b=baseline[i]+(h-caps[i])/2;
        g[0]=(short)h; g[1]=(short)h; g[2]=(short)(h+2); g[3]=0; memcpy((char *)g+8,&b,4);
    }
    metrics_ready=1;
    snprintf(line,sizeof line,"Glyph metrics PASS cells=24+2 style prefixes capitals=%d,%d,%d,%d baseline=%d,%d,%d,%d; font pixels/UV unchanged\r\n",
             caps[0],caps[1],caps[2],caps[3],baseline[0],baseline[1],baseline[2],baseline[3]); log_line(line);
    return 1;
}
int pg_render_ready(void *context) {
    IDirect3DDevice8 *candidate; D3DLOCKED_RECT lock; unsigned at=0,x=0,y=0; HRESULT hr;
    if(failed || !readable(context,0x1f4)) return 0;
    candidate=*(IDirect3DDevice8 **)((char *)context+0x144);
    if(!readable(candidate,sizeof(void *)) || !readable(candidate->lpVtbl,sizeof *candidate->lpVtbl)) return 0;
    if(device && candidate!=device) { pg_render_disable("device identity changed"); return 0; }
    if(!device) device=candidate;
    if(!metrics_ready) {
        DWORD now=GetTickCount();
        if(next_metrics_attempt && (LONG)(now-next_metrics_attempt)<0) return 0;
        next_metrics_attempt=now+500;
        if(!publish_metrics()) return 0;
    }
    if(texture) return 1;
    hr=IDirect3DDevice8_CreateTexture(device,512,512,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&texture);
    if(FAILED(hr)) { pg_render_disable("CreateTexture"); return 0; }
    hr=IDirect3DTexture8_LockRect(texture,0,&lock,NULL,0);
    if(FAILED(hr) || lock.Pitch<2048) { pg_render_disable("atlas LockRect"); return 0; }
    while(at+6<=sizeof mc_atlas_rle && y<512) {
        unsigned n=mc_atlas_rle[at]|(unsigned)mc_atlas_rle[at+1]<<8; DWORD pixel; unsigned k;
        memcpy(&pixel,mc_atlas_rle+at+2,4); at+=6;
        for(k=0;k<n && y<512;k++) {
            memcpy((char *)lock.pBits+y*lock.Pitch+x*4,&pixel,4);
            if(++x==512) { x=0; y++; }
        }
    }
    IDirect3DTexture8_UnlockRect(texture,0);
    if(y!=512 || x || at!=sizeof mc_atlas_rle) { pg_render_disable("atlas decoder extent"); return 0; }
    log_line("Glyph atlas LOADED 512x512 A8R8G8B8 MANAGED; 48 own cells; 4px padding\r\n");
    return 1;
}
/* Low byte = zero-advance style prefix; high byte = logical button. */
int pg_render_code(unsigned index) {
    return pg_render_active() && index<48?atlas_to_code[index%24]*256+atlas_to_code[24+index/24]:0;
}
static int usable_sink(void *writer,unsigned vertices) {
    uintptr_t batch; unsigned slot,capacity,base,count; int valid;
    if(!readable(writer,12)) return 0;
    batch=*(uintptr_t *)writer;
    if(!readable((void *)batch,0x24)) return 0;
    slot=*(unsigned *)(batch+0x10); capacity=*(unsigned *)(batch+0xc);
    base=*(unsigned *)(batch+0x14); count=*(unsigned *)(batch+0x20);
    /* Retail constructor 005843A0: exactly two array pointers (+4/+8),
       capacity +C, index +10 and emitter vtable 0069C904. Other UI passes
       can use another sink; keep readable text in that run instead. */
    valid=*(uintptr_t *)batch==0x69c904u && slot<2 &&
        (uint64_t)base+count+vertices<capacity && *(uintptr_t *)(batch+4+slot*4)!=0;
    if(!valid && sink_notes++<4) {
        char line[220]; snprintf(line,sizeof line,"Glyph run TEXT fallback sink=%p vtable=%p slot=%u base=%u count=%u capacity=%u; renderer stays active\r\n",
            (void *)batch,*(void **)batch,slot,base,count,capacity); log_line(line);
    }
    return valid;
}
const WCHAR *pg_render_safe_text(const WCHAR *s,void *owner,void *writer) {
    unsigned i,n=0,font=~0u,style=0,native=0,length=0; int can_render=pg_render_active();
    if(!metrics_ready || !readable(s,1024)) return s;
    if(owner && readable((char *)owner+0x2140c,4)) font=*(unsigned *)((char *)owner+0x2140c);
    if(font>=4 || *(short *)(font_table_address+font*0x1c18u+0x18+'A'*28+2)<=0) can_render=0;
    while(length<512 && s[length]) { if(s[length]<256 && code_to_atlas[s[length]]<24) native++; length++; }
    if(!native) return s;
    if(native>QUEUE_MAX-queued || length==512) can_render=0;
    if(can_render && !usable_sink(writer,length*6)) can_render=0;
    if(can_render) return s;
    for(i=0;i<512 && s[i] && n+40<2048;i++) {
        unsigned c=s[i],index=c<256?code_to_atlas[c]:255;
        if(index==24 || index==25) { style=index-24; continue; }
        if(index<24) {
            const char *name=pg_control_name((McControl)index,style?PG_PLAYSTATION:PG_XBOX);
            fallback_text[n++]='[';
            while(*name) fallback_text[n++]=(unsigned char)*name++;
            fallback_text[n++]=']';
        } else fallback_text[n++]=s[i];
    }
    fallback_text[n]=0; return fallback_text;
}
int pg_render_text_begin(const WCHAR *s,void *writer,void *owner) {
    unsigned n,native=0,draws=0; unsigned font;
    cursor=NULL;
    if(!pg_render_active() || !readable(s,1024) || !readable(writer,12) || !owner ||
       !readable((char *)owner+0x2140c,4)) return 0;
    font=*(unsigned *)((char *)owner+0x2140c);
    if(font>=4 || *(short *)(font_table_address+font*0x1c18u+0x18+'A'*28+2)<=0) return 0;
    for(n=0;n<512 && s[n];n++) if(s[n]<256 && s[n]!=' ' && s[n]!='\t') {
        draws++; if(code_to_atlas[s[n]]<24) native++;
    }
    if(n==512 || !native || native>QUEUE_MAX-queued || !usable_sink(writer,draws*6)) return 0;
    cursor=s; cursor_index=0; expected=draws; emitted=0; cursor_style=0; pending_inline=QUEUE_MAX;
    return 1;
}
void pg_render_text_end(void) {
    if(cursor && emitted!=expected) pg_render_disable("wchar/quad cursor mismatch");
    cursor=NULL;
    pending_inline=QUEUE_MAX;
}
int pg_render_quad(void *writer,float x0,float y0,float x1,float y1) {
    unsigned c=0,index,slot,base,count; uintptr_t batch,array;
    if(!cursor) return 0;
    while(cursor[cursor_index]) { c=cursor[cursor_index++]; if(c<256 && c!=' ' && c!='\t') break; }
    emitted++;
    if(c>=256 || (index=code_to_atlas[c])==255) {
        /* Tighten an inline icon's visual gap to the next real letter. The
           native text/wrapping/advance remains unchanged; move only our sprite
           into already unused whitespace on the same line. */
        if(pending_inline<queued) {
            GlyphQuad *q=&queue[pending_inline];
            float height=fabsf(q->y1-q->y0),gap=x0-q->x1;
            float center_delta=fabsf((y0+y1-q->y0-q->y1)*0.5f),wanted=height*0.12f;
            if(q->x1>q->x0 && x1>x0 && gap>wanted && gap<height*4 && center_delta<height*0.5f) {
                q->x0+=gap-wanted; q->x1+=gap-wanted;
            }
        }
        pending_inline=QUEUE_MAX;
        return 0;
    }
    if(index==24 || index==25) { cursor_style=(index-24)*24; return 1; }
    index+=cursor_style;
    if(!readable(writer,12) || queued==QUEUE_MAX) { pg_render_disable("quad writer/capacity"); return 0; }
    batch=*(uintptr_t *)writer;
    if(!readable((void *)batch,0x24)) { pg_render_disable("batch layout"); return 0; }
    slot=*(unsigned *)(batch+0x10); base=*(unsigned *)(batch+0x14); count=*(unsigned *)(batch+0x20);
    if(slot>=2 || !readable((void *)(batch+4+slot*4),4)) { pg_render_disable("vertex array slot changed within accepted text run"); return 0; }
    array=*(uintptr_t *)(batch+4+slot*4);
    if(!array || !isfinite(x0) || !isfinite(x1) || !isfinite(y0) || !isfinite(y1)) { pg_render_disable("quad coordinates"); return 0; }
    pending_inline=queued;
    queue[queued++]=(GlyphQuad){array,base+count,index,x0,y0,x1,y1,*(DWORD *)((char *)writer+8)};
    return 1;
}
void pg_render_batch(void *context,unsigned type,int count,int start) {
    current_context=(uintptr_t)context; current_valid=type==4 && count>0 && start>=0;
    current_start=(unsigned)start; current_end=current_start+(unsigned)count+2;
}
static HRESULT sprite(GlyphQuad *q) {
    const float *uv=mc_glyph_uv[q->atlas];
    Vertex v[6]={
        {q->x0,0,q->y0,q->color,uv[0],uv[1]}, {q->x1,0,q->y0,q->color,uv[2],uv[1]}, {q->x0,0,q->y1,q->color,uv[0],uv[3]},
        {q->x0,0,q->y1,q->color,uv[0],uv[3]}, {q->x1,0,q->y0,q->color,uv[2],uv[1]}, {q->x1,0,q->y1,q->color,uv[2],uv[3]}};
    return IDirect3DDevice8_DrawPrimitiveUP(device,D3DPT_TRIANGLELIST,2,v,sizeof(Vertex));
}
void pg_render_submit(void *context) {
    uintptr_t array; unsigned i,matching=0; HRESULT hr=S_OK;
    if(!current_valid || current_context!=(uintptr_t)context || !queued || !pg_render_active()) return;
    array=*(uintptr_t *)((char *)context+0x1f0);
    for(i=0;i<queued;i++) if(queue[i].array==array && queue[i].vertex>=current_start && queue[i].vertex+6<=current_end) matching++;
    if(!matching) return;
    if(!state_block) hr=IDirect3DDevice8_CreateStateBlock(device,D3DSBT_ALL,&state_block);
    else hr=IDirect3DDevice8_CaptureStateBlock(device,state_block);
    if(FAILED(hr)) { pg_render_disable("state capture"); return; }
#define SET(call) do { if(SUCCEEDED(hr)) hr=(call); } while(0)
    SET(IDirect3DDevice8_SetVertexShader(device,D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1));
    SET(IDirect3DDevice8_SetPixelShader(device,0));
    SET(IDirect3DDevice8_SetTexture(device,0,(IDirect3DBaseTexture8 *)texture));
    SET(IDirect3DDevice8_SetRenderState(device,D3DRS_LIGHTING,FALSE));
    SET(IDirect3DDevice8_SetRenderState(device,D3DRS_FOGENABLE,FALSE));
    SET(IDirect3DDevice8_SetRenderState(device,D3DRS_CULLMODE,D3DCULL_NONE));
    SET(IDirect3DDevice8_SetRenderState(device,D3DRS_ZENABLE,FALSE));
    SET(IDirect3DDevice8_SetRenderState(device,D3DRS_ZWRITEENABLE,FALSE));
    SET(IDirect3DDevice8_SetRenderState(device,D3DRS_ALPHABLENDENABLE,TRUE));
    SET(IDirect3DDevice8_SetRenderState(device,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA));
    SET(IDirect3DDevice8_SetRenderState(device,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA));
    SET(IDirect3DDevice8_SetRenderState(device,D3DRS_ALPHATESTENABLE,FALSE));
    SET(IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_COLOROP,D3DTOP_MODULATE));
    SET(IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_COLORARG1,D3DTA_TEXTURE));
    SET(IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_COLORARG2,D3DTA_DIFFUSE));
    SET(IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_ALPHAOP,D3DTOP_MODULATE));
    SET(IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE));
    SET(IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE));
    SET(IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_TEXCOORDINDEX,0));
    SET(IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_DISABLE));
    SET(IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_MINFILTER,D3DTEXF_LINEAR));
    SET(IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_MAGFILTER,D3DTEXF_LINEAR));
    SET(IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_MIPFILTER,D3DTEXF_NONE));
    SET(IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP));
    SET(IDirect3DDevice8_SetTextureStageState(device,0,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP));
    SET(IDirect3DDevice8_SetTextureStageState(device,1,D3DTSS_COLOROP,D3DTOP_DISABLE));
    for(i=0;i<queued;) {
        GlyphQuad *q=&queue[i];
        if(q->array==array && q->vertex>=current_start && q->vertex+6<=current_end) {
            SET(sprite(q)); queued--;
            memmove(queue+i,queue+i+1,(queued-i)*sizeof *queue);
        } else i++;
    }
    if(FAILED(IDirect3DDevice8_ApplyStateBlock(device,state_block))) { pg_render_disable("state restore"); return; }
    if(FAILED(hr)) { pg_render_disable("D3D8 glyph submission"); return; }
    if(draw_count++<6) { char line[160]; snprintf(line,sizeof line,"Glyph batch SUBMITTED quads=%u array=%p range=%u..%u state_restored=1\r\n",matching,(void *)array,current_start,current_end); log_line(line); }
#undef SET
}
void pg_render_batch_end(void) { current_valid=0; current_context=0; }

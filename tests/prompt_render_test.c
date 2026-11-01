/* Owned fixture exercises the actual retail-boundary adapter, not game assets. */
#include "../src/prompt_render.c"
#undef NDEBUG
#include <assert.h>
void analog_log_event(const char *s) { fputs(s,stdout); }
typedef struct { DWORD fvf,shader,rs[256],tss[8][32]; void *tex; } State;
static State gpu,snapshot;
static int submissions,refuse_capture;
static Vertex submitted[6];
static HRESULT WINAPI create_block(IDirect3DDevice8 *d,D3DSTATEBLOCKTYPE t,DWORD *out) {
    (void)d; assert(t==D3DSBT_ALL); *out=7; snapshot=gpu; return refuse_capture?E_FAIL:S_OK;
}
static HRESULT WINAPI capture(IDirect3DDevice8 *d,DWORD t) { (void)d; assert(t==7); snapshot=gpu; return refuse_capture?E_FAIL:S_OK; }
static HRESULT WINAPI restore(IDirect3DDevice8 *d,DWORD t) { (void)d; assert(t==7); gpu=snapshot; return S_OK; }
static HRESULT WINAPI fvf(IDirect3DDevice8 *d,DWORD v) { (void)d; gpu.fvf=v; return S_OK; }
static HRESULT WINAPI shader(IDirect3DDevice8 *d,DWORD v) { (void)d; gpu.shader=v; return S_OK; }
static HRESULT WINAPI render_state(IDirect3DDevice8 *d,D3DRENDERSTATETYPE t,DWORD v) { (void)d; gpu.rs[t]=v; return S_OK; }
static HRESULT WINAPI stage_state(IDirect3DDevice8 *d,DWORD stage,D3DTEXTURESTAGESTATETYPE t,DWORD v) { (void)d; gpu.tss[stage][t]=v; return S_OK; }
static HRESULT WINAPI tex(IDirect3DDevice8 *d,DWORD stage,IDirect3DBaseTexture8 *t) { (void)d; assert(stage==0); gpu.tex=t; return S_OK; }
static HRESULT WINAPI submit(IDirect3DDevice8 *d,D3DPRIMITIVETYPE type,UINT n,const void *vertices,UINT stride) {
    (void)d; assert(type==D3DPT_TRIANGLELIST && n==2 && stride==sizeof(Vertex));
    assert(gpu.fvf==(D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1));
    assert(!gpu.shader && !gpu.rs[D3DRS_ZENABLE] && !gpu.rs[D3DRS_LIGHTING]);
    assert(gpu.rs[D3DRS_ALPHABLENDENABLE] && gpu.rs[D3DRS_SRCBLEND]==D3DBLEND_SRCALPHA);
    assert(gpu.tex==texture); memcpy(submitted,vertices,sizeof submitted); submissions++; return S_OK;
}
static IDirect3DDevice8Vtbl vtable={.CreateStateBlock=create_block,.CaptureStateBlock=capture,.ApplyStateBlock=restore,
    .SetVertexShader=fvf,.SetPixelShader=shader,.SetRenderState=render_state,.SetTextureStageState=stage_state,
    .SetTexture=tex,.DrawPrimitiveUP=submit};
static IDirect3DDevice8 mock={&vtable};
static unsigned char owner[0x21500],context[0x500],batch[256];
static uintptr_t writer[3];
static WCHAR wide[512];
int main(void) {
    setvbuf(stdout,NULL,_IONBF,0);
    void *fonts=VirtualAlloc(NULL,0x20000,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
    unsigned f,ch; int token; State before;
    assert(fonts); font_table_address=(uintptr_t)fonts+0x100; *(uintptr_t *)font_table_address=0x69d69c;
    for(f=0;f<3;f++) for(ch='A';ch<='Z';ch++) {
        short *g=(void *)(font_table_address+f*0x1c18+0x18+ch*28); int b=16+(int)f*6;
        g[0]=12; g[1]=(short)(18+f*6); g[2]=12; memcpy((char *)g+8,&b,4);
    }
    assert(publish_metrics());
    {
        short *g=(void *)(font_table_address+0x18+atlas_to_code[MC_SOUTH]*28);
        assert(g[0]==27 && g[1]==27 && g[2]==29 && g[3]==0);
        assert(*(int *)((char *)g+8)==20);
        assert(*(float *)((char *)g+12)==0); /* The atlas of the font is never patched. */
    }
    device=&mock; texture=(IDirect3DTexture8 *)&mock;
    assert(pg_render_code(999)==0);
    token=pg_render_code(24+MC_SOUTH); assert(token);
    wide[0]=(WCHAR)(token&255); wide[1]=(WCHAR)(token>>8);
    *(uintptr_t *)batch=0x69c904;
    *(uintptr_t *)(batch+4)=0x12345678; *(unsigned *)(batch+0xc)=4096; *(unsigned *)(batch+0x14)=10;
    writer[0]=(uintptr_t)batch; writer[2]=0xffabcdef;
    assert(pg_render_text_begin(wide,writer,owner));
    assert(pg_render_quad(writer,0,0,0,0));
    assert(queued==0); *(unsigned *)(batch+0x20)=6;
    assert(pg_render_quad(writer,10,30,40,20));
    pg_render_text_end(); assert(!failed && queued==1 && queue[0].atlas==24+MC_SOUTH);
    *(uintptr_t *)(context+0x1f0)=0x99999999;
    pg_render_batch(context,4,10,10); pg_render_submit(context); pg_render_batch_end();
    assert(!submissions && queued==1); /* No glyph leaks into an unrelated draw. */
    *(uintptr_t *)(context+0x1f0)=0x12345678;
    memset(&gpu,0x19,sizeof gpu); before=gpu;
    pg_render_batch(context,4,10,10); pg_render_submit(context); pg_render_batch_end();
    assert(submissions==1 && queued==0 && !memcmp(&gpu,&before,sizeof gpu));
    assert(submitted[0].x==10 && submitted[0].y==0 && submitted[0].z==30);
    assert(submitted[2].x==10 && submitted[2].z==20);
    assert(submitted[0].color==0xffabcdef);
    assert(submitted[0].u==mc_glyph_uv[28][0] && submitted[0].v==mc_glyph_uv[28][1]);
    assert(submitted[2].v==mc_glyph_uv[28][3]);
    /* Both sprite triangles share exactly the same edge, including UV/color.
       No diagonal gap, overlapping strip order or UV break is introduced. */
    assert(!memcmp(&submitted[2],&submitted[3],sizeof(Vertex)));
    assert(!memcmp(&submitted[1],&submitted[4],sizeof(Vertex)));
    assert(submitted[5].x==submitted[1].x && submitted[5].z==submitted[2].z);
    assert(submitted[5].u==submitted[1].u && submitted[5].v==submitted[2].v);
    /* A following word closes only the visual gap; native geometry stays put. */
    token=pg_render_code(MC_NORTH); wide[0]=(WCHAR)(token&255); wide[1]=(WCHAR)(token>>8);
    wide[2]=' '; wide[3]='C'; wide[4]=0;
    assert(pg_render_text_begin(wide,writer,owner));
    assert(pg_render_quad(writer,0,0,0,0));
    assert(pg_render_quad(writer,10,30,30,10));
    assert(!pg_render_quad(writer,45,27,55,13));
    pg_render_text_end();
    assert(queued==1 && fabsf(queue[0].x1-42.6f)<0.001f);
    assert(fabsf(queue[0].x0-22.6f)<0.001f);
    queued=0;
    /* Xbox and PlayStation can share a run; prefixes never steal a slot. */
    token=pg_render_code(MC_NORTH); wide[0]=(WCHAR)(token&255); wide[1]=(WCHAR)(token>>8);
    token=pg_render_code(24+MC_NORTH); wide[2]=(WCHAR)(token&255); wide[3]=(WCHAR)(token>>8); wide[4]=0;
    assert(pg_render_text_begin(wide,writer,owner));
    assert(pg_render_quad(writer,0,0,0,0)); assert(pg_render_quad(writer,10,30,30,10));
    assert(pg_render_quad(writer,0,0,0,0)); assert(pg_render_quad(writer,40,30,60,10));
    pg_render_text_end(); assert(queued==2 && queue[0].atlas==MC_NORTH && queue[1].atlas==24+MC_NORTH);
    queued=0;
    *(unsigned *)(batch+0x10)=~0u;
    assert(!wcscmp(pg_render_safe_text(wide,owner,writer),L"[Y][Triangle]"));
    assert(!pg_render_text_begin(wide,writer,owner) && pg_render_active());
    *(unsigned *)(batch+0x10)=0;
    {
        WCHAR help[512]={0};
        for(unsigned style=0;style<2;style++) for(unsigned i=0;i<MC_CONTROL_COUNT;i++) {
            int code=pg_render_code(style*24+i);
            assert(code && code_to_atlas[(unsigned)code&255]==24+style);
            assert(code_to_atlas[(unsigned)code>>8]==i);
        }
        int left=pg_render_code(MC_DPAD_LEFT),right=pg_render_code(24+MC_DPAD_RIGHT);
        help[0]=(WCHAR)(left&255); help[1]=(WCHAR)((unsigned)left>>8);
        help[2]=(WCHAR)(right&255); help[3]=(WCHAR)((unsigned)right>>8);
        *(uintptr_t *)batch=0x6a4c4c; /* Incompatible help writer stays textual. */
        assert(!wcscmp(pg_render_safe_text(help,owner,writer),L"[D-pad Left][D-pad Right]"));
        assert(pg_render_active());
    }
    *(uintptr_t *)batch=0x69c8e4;
    assert(!wcscmp(pg_render_safe_text(wide,owner,writer),L"[Y][Triangle]"));
    assert(pg_render_active());
    *(uintptr_t *)batch=0x69c904;
    /* Unknown/corrupt render conditions become textual names, never private tokens. */
    pg_render_disable("fixture missing atlas");
    assert(!wcscmp(pg_render_safe_text(wide,owner,writer),L"[Y][Triangle]"));
    assert(!pg_render_code(4));
    VirtualFree(fonts,0,MEM_RELEASE);
    puts("PASS: retail font header offset; 26 empty cells; mixed styles; upright UV/color/plane; inline visual gap; vertex-range matching; alpha setup; full state restore; missing-atlas text fallback");
    return 0;
}

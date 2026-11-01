#ifndef XML2_PROMPT_RENDER_H
#define XML2_PROMPT_RENDER_H
#include "xml2_analog.h"
int pg_render_ready(void *context);
int pg_render_code(unsigned atlas_index);
int pg_render_text_begin(const WCHAR *string,void *writer,void *owner);
const WCHAR *pg_render_safe_text(const WCHAR *string,void *owner,void *writer);
void pg_render_text_end(void);
int pg_render_quad(void *writer,float x0,float y0,float x1,float y1);
void pg_render_batch(void *context,unsigned type,int count,int start);
void pg_render_submit(void *context);
void pg_render_batch_end(void);
void pg_render_disable(const char *reason);
int pg_render_active(void);
#endif

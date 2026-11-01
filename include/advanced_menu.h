#ifndef XML2_ADVANCED_MENU_H
#define XML2_ADVANCED_MENU_H
#include "controller_model.h"
void mc_advanced_prepare(uintptr_t image,const WCHAR *ini,const McCore *core);
void mc_advanced_state(int role,const void *state,unsigned size,uintptr_t caller,const GUID *instance);
uintptr_t mc_advanced_manager(void);
int mc_advanced_enabled(void);
void mc_advanced_lost(const GUID *instance);
#endif

/* The expansion delegates forwarding to the unchanged v0.1.0 proxy source. */
#include "controller_core.h"

#define DirectInputCreateA legacy_create_a
#define DirectInputCreateW legacy_create_w
#define DirectInputCreateEx legacy_create_ex
#include "dinput_proxy.c"
#undef DirectInputCreateA
#undef DirectInputCreateW
#undef DirectInputCreateEx

HRESULT WINAPI DirectInputCreateA(HINSTANCE instance,DWORD version,void **out,void *outer) {
    controller_core_prepare(self_module);
    return legacy_create_a(instance,version,out,outer);
}

HRESULT WINAPI DirectInputCreateW(HINSTANCE instance,DWORD version,void **out,void *outer) {
    controller_core_prepare(self_module);
    return legacy_create_w(instance,version,out,outer);
}

HRESULT WINAPI DirectInputCreateEx(HINSTANCE instance,DWORD version,const IID *iid,void **out,void *outer) {
    HRESULT hr;
    controller_core_prepare(self_module);
    hr=legacy_create_ex(instance,version,iid,out,outer);
    if (SUCCEEDED(hr)&&out&&*out) controller_core_created7(iid,*out);
    return hr;
}

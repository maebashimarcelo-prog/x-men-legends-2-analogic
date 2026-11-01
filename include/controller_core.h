#ifndef XML2_CONTROLLER_CORE_H
#define XML2_CONTROLLER_CORE_H

#include "xml2_analog.h"

/* Optional expansion: the default build remains XML2 True Analog v0.1.0. */
void controller_core_prepare(HMODULE module);
void controller_core_created7(const IID *iid, void *object);

#endif

#ifndef VIZ_TAPE_MODULE_EXPORTS_H
#define VIZ_TAPE_MODULE_EXPORTS_H
#include <psploadcore.h>
#include <stdint.h>

/* Read a private user module's export table without a CFW lookup. variable
 * selects the variable (1) or function (0) NID section. library is a trusted
 * caller-owned string. The caller must validate the returned address and
 * complete object extent before dereferencing it or installing a callback. */
uintptr_t tape_find_export(const SceModule *module, const char *library,
                           uint32_t nid, int variable);
#endif

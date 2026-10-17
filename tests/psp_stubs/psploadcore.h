#ifndef TEST_PSP_LOADCORE_H
#define TEST_PSP_LOADCORE_H
#include "pspkernel.h"
/* Native pointer widths let host fixtures live in ASan-managed allocations. */
typedef struct {
    const char *libname;
    unsigned char version[2];
    unsigned short attribute;
    unsigned char len, vstubcount;
    unsigned short stubcount;
    void *entrytable;
} SceLibraryEntryTable;
SceModule *sceKernelFindModuleByName(const char *);
SceModule *sceKernelFindModuleByUID(SceUID);
#endif

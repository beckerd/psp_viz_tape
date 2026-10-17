/* Minimal host ABI for tests that include plugin.c. PSP builds use PSPSDK. */
#ifndef TEST_PSP_KERNEL_H
#define TEST_PSP_KERNEL_H
#include <stddef.h>
#include <stdint.h>
#include "pspiofilemgr.h"
typedef unsigned SceSize;
/* Address fields use host pointer width; fixtures still use PSP addresses. */
typedef struct {
    unsigned nsegment, text_size, data_size, bss_size;
    uintptr_t text_addr, segmentaddr[4];
    unsigned segmentsize[4];
    char modname[32];
    void *ent_top;
    unsigned ent_size;
} SceModule;
#define PSP_MODULE_KERNEL 0
#define PSP_MODULE_INFO(...)
#define PSP_NO_CREATE_MAIN_THREAD(...)
int sceKernelCpuSuspendIntr(void);
void sceKernelCpuResumeIntr(int);
void sceKernelDcacheWritebackAll(void);
int sceKernelDelayThread(unsigned);
int sceKernelExitDeleteThread(int);
unsigned sceKernelDevkitVersion(void);
SceUID sceKernelCreateThread(const char *, int (*)(SceSize, void *), int, int, int, void *);
int sceKernelStartThread(SceUID, SceSize, void *);
int sceKernelDeleteThread(SceUID);
#endif

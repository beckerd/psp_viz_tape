#include <pspkernel.h>
#include <psploadcore.h>
#include <pspsysmem_kernel.h>
#include <pspintrman.h>
#include <pspinit.h>
#include <pspusb.h>
#include <pspmodulemgr_kernel.h>
#include <systemctrl.h>
#include <stdio.h>
#include <string.h>
#include "profile.h"
#include "runtime.h"
#include "safety.h"
#include "icon.h"
#include "renderer_payload.h"
#include "module_loader.h"
#include "module_exports.h"

PSP_MODULE_INFO("viz_tape", PSP_MODULE_KERNEL, 1, 3);
#define VIZ_TAPE_VERSION "1.3"
PSP_NO_CREATE_MAIN_THREAD();

static STMOD_HANDLER previous_handler;
static volatile unsigned event, patched_base, seek_event;
static volatile unsigned icon_event;
static volatile int renderer_started;
static const char *const icon_events[] = {
    "ICON skipped: resource missing or failed validation",
    "ICON ready: waiting for Music player",
    "ICON cassette resource selected in RAM",
    "ICON skipped: Music player layout/path mismatch",
    "ICON created visualizer.rco from the stock resource",
    "ICON not created: stock resource differs (unsupported firmware); stock icon kept"
};
static const char *const seek_events[] = {
    "", "SEEK fast-forward/rewind state found", "SEEK skipped: playback state layout mismatch"
};
static PspSysmemPartitionInfo user_partition;
static const char *const events[] = {
    "waiting for visualizer", "PATCHED final brown-line slot 7",
    "SKIPPED: firmware layout mismatch", "SKIPPED: renderer export/ABI mismatch",
    "SKIPPED: PAF exports do not match native imports", "SKIPPED: draw slot changed",
    "waiting for embedded renderer"
};

static volatile int guarded;

/* An error result is treated as "maybe active": never write to the card then. */
static int usb_active(void)
{
    int usb = sceUsbGetState();
    return usb < 0 || (usb & PSP_USB_ACTIVATED);
}

static void log_message(const char *message)
{
    if (usb_active()) return;
    SceUID fd = sceIoOpen(TAPE_LOG_PATH, PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND, 0777);
    if (fd >= 0) { sceIoWrite(fd, message, strlen(message)); sceIoClose(fd); }
}
#define report(...) do { char msg[256]; snprintf(msg, sizeof msg, __VA_ARGS__); log_message(msg); } while (0)

/* Persist the last stage before entering the firmware module manager. */
static void checkpoint(const char *message)
{
    log_message(message);
    if (!usb_active()) sceIoSync(tape_paths.device, 0);
}

static int module_contains(SceModule *m, uintptr_t p, size_t size)
{
    unsigned i;
    if (!m || m->nsegment < 1 || m->nsegment > 4) return 0;
    for (i = 0; i < m->nsegment; ++i)
        if (tape_range_contains(m->segmentaddr[i], m->segmentsize[i], p, size)) return 1;
    return 0;
}

static uintptr_t resolve_paf(SceModule *viz, SceModule *paf, unsigned nid, unsigned stub_offset)
{
    uintptr_t p = sctrlHENFindFunction("scePaf_Module", "scePaf", nid);
    uintptr_t stub = viz->text_addr + stub_offset;
    if (!tape_range_contains(paf->text_addr, paf->text_size, p, 4) ||
        !tape_stub_matches((const uint32_t *)stub, stub, p)) return 0;
    return p;
}

static void patch_visualizer(SceModule *module)
{
    SceModule *renderer, *paf;
    uint32_t *slot;
    uintptr_t draw, bridge_address;
    volatile TapeBridge *bridge;
    TapeBindings bindings;
    int irq;
    if (!renderer_started) { event = 6; return; }
    if (module->nsegment != 2 || module->text_size != TAPE_TEXT_SIZE ||
        module->data_size != TAPE_DATA_SIZE || module->bss_size != TAPE_BSS_SIZE ||
        module->segmentaddr[0] != module->text_addr ||
        module->segmentsize[0] != TAPE_TEXT_SIZE ||
        !tape_user_range(module->text_addr, TAPE_TEXT_SIZE) ||
        !tape_profile_matches((void *)module->text_addr, module->text_size, module->text_addr)) {
        event = 2; return;
    }
    renderer = sceKernelFindModuleByName(TAPE_RENDERER_MODULE);
    paf = sceKernelFindModuleByName("scePaf_Module");
    /* Resolve both private exports locally. No CFW fallback may bypass the
     * table checks; PAF lookup below still uses the CFW's NID translation. */
    draw = tape_find_export(renderer, TAPE_RENDERER_LIBRARY, 0xeab11153, 0);
    bridge_address = tape_find_export(renderer, TAPE_RENDERER_LIBRARY, 0x99f748f8, 1);
    if (!renderer || !paf ||
        !tape_range_contains(renderer->text_addr, renderer->text_size, draw, 4) ||
        !module_contains(renderer, bridge_address, sizeof(TapeBridge))) {
        event = 3; return;
    }
    bridge = (volatile TapeBridge *)bridge_address;
    if (bridge->abi != TAPE_ABI || bridge->size != sizeof(TapeBridge) ||
        bridge->status.abi != TAPE_ABI) { event = 3; return; }
    bindings.abi = TAPE_ABI;
    bindings.user_base = user_partition.startaddr;
    bindings.user_size = user_partition.memsize;
    bindings.draw = (TapeDrawArray)resolve_paf(module, paf, TAPE_NID_DRAW, TAPE_STUB_DRAW);
    bindings.enable = (TapeStateFunction)resolve_paf(module, paf, TAPE_NID_ENABLE, TAPE_STUB_ENABLE);
    bindings.disable = (TapeStateFunction)resolve_paf(module, paf, TAPE_NID_DISABLE, TAPE_STUB_DISABLE);
    bindings.blend = (TapeBlendFunction)resolve_paf(module, paf, TAPE_NID_BLEND, TAPE_STUB_BLEND);
    if (!bindings.draw || !bindings.enable || !bindings.disable || !bindings.blend) {
        event = 4; return;
    }
    /* Optional: without a verified state word, seeking simply winds normally. */
    {
        uintptr_t state = tape_play_state_address((void *)module->text_addr,
                                                 module->text_size, module->text_addr);
        bindings.play_state = state && module_contains(module, state, 4) &&
            !tape_range_contains(module->text_addr, module->text_size, state, 4) ?
            (const volatile uint32_t *)state : 0;
        seek_event = bindings.play_state ? 1 : 2;
    }
    slot = (uint32_t *)(module->text_addr + TAPE_DRAW_SLOT);
    irq = sceKernelCpuSuspendIntr();
    if (*slot != module->text_addr + TAPE_ORIGINAL_DRAW) {
        sceKernelCpuResumeIntr(irq); event = 5; return;
    }
    bridge->ready = 0;
    bridge->api = bindings;
    bridge->ready = TAPE_READY;
    *slot = draw;
    sceKernelDcacheWritebackAll();
    sceKernelCpuResumeIntr(irq);
    patched_base = module->text_addr;
    event = 1;
}

static int on_module_start(SceModule *module)
{
    int result = previous_handler ? previous_handler(module) : 0;
    if (module && strcmp(module->modname, "music_player_module") == 0) {
        int irq;
        icon_event = 3;
        if (module->nsegment == 2 && module->text_size == TAPE_MUSIC_TEXT_SIZE &&
            module->data_size == TAPE_MUSIC_DATA_SIZE && module->bss_size == TAPE_MUSIC_BSS_SIZE &&
            module->segmentaddr[0] == module->text_addr &&
            module->segmentsize[0] == TAPE_MUSIC_TEXT_SIZE &&
            tape_user_range(module->text_addr, TAPE_MUSIC_TEXT_SIZE)) {
            /* USB may have replaced or removed the file since startup. Check
             * it for every new Music module, even after the reporter exits.
             * This bounded read runs with interrupts enabled; no files are
             * created from the module-start callback. */
            icon_event = 0;
            if (!usb_active() && tape_icon_resource_ready() && !usb_active()) {
                irq = sceKernelCpuSuspendIntr();
                icon_event = 3;
                if (tape_icon_redirect((void *)module->text_addr, module->text_size, module->text_addr)) {
                    sceKernelDcacheWritebackAll();
                    icon_event = 2;
                }
                sceKernelCpuResumeIntr(irq);
            }
        }
    }
    if (module && strcmp(module->modname, "visualizer_module") == 0) patch_visualizer(module);
    return result;
}

/* Module-manager operations cannot be nested inside module_start or its
 * handler. The worker queues this load after our own module_start returns. */
static void start_renderer(void)
{
    SceKernelLMOption options = {
        .size = sizeof(SceKernelLMOption),
        .mpidtext = PSP_MEMORY_PARTITION_USER, .mpiddata = PSP_MEMORY_PARTITION_USER
    };
    SceUID uid, buffer_id;
    void *buffer;
    SceModule *viz;
    int result, status = -1, irq;
    /* Avoid duplicate instances when the plugin is accidentally loaded twice. */
    if (sceKernelFindModuleByName(TAPE_RENDERER_MODULE)) {
        report("RENDERER skipped: runtime already loaded; restart with one Viz Tape entry\n");
        return;
    }
    /* The plain-PRX loader copies the greater of the file length and loaded
     * segment span (including BSS). Do not let it read past our embedded
     * bytes into unrelated kernel memory. Release this user-memory staging
     * block as soon as the synchronous load completes. */
    buffer_id = sceKernelAllocPartitionMemory(PSP_MEMORY_PARTITION_USER, "VizTapeLoad",
                                              PSP_SMEM_High, tape_renderer_buffer_size, 0);
    if (buffer_id < 0) {
        report("RENDERER buffer allocation failed: %08X; stock visualizer kept\n", (unsigned)buffer_id);
        return;
    }
    buffer = sceKernelGetBlockHeadAddr(buffer_id);
    if (!buffer || ((uintptr_t)buffer & 63)) {
        sceKernelFreePartitionMemory(buffer_id);
        report("RENDERER buffer unavailable or unaligned; stock visualizer kept\n");
        return;
    }
    memset(buffer, 0, tape_renderer_buffer_size);
    memcpy(buffer, tape_renderer_prx, tape_renderer_prx_size);
    checkpoint("RENDERER loading embedded runtime (size-first firmware ABI)\n");
    uid = tape_load_module_buffer(tape_renderer_prx_size, buffer, 0, &options);
    sceKernelFreePartitionMemory(buffer_id);
    if (uid < 0) {
        report("RENDERER load failed: %08X; stock visualizer kept\n", (unsigned)uid);
        return;
    }
    checkpoint("RENDERER starting embedded runtime\n");
    result = sceKernelStartModule(uid, 0, 0, &status, 0);
    if (result <= 0 || status != 0) {
        /* A nonresident/failed start may already have unloaded the module. */
        if (sceKernelFindModuleByUID(uid)) sceKernelUnloadModule(uid);
        report("RENDERER start failed: %08X status=%08X; stock visualizer kept\n",
               (unsigned)result, (unsigned)status);
        return;
    }
    /* A Music session can race the worker. Cover an already loaded visualizer
     * as well as future module-start callbacks. Keep lookup and the complete
     * patch check atomic against unload; this path has no I/O or waits. */
    irq = sceKernelCpuSuspendIntr();
    renderer_started = 1;
    viz = sceKernelFindModuleByName("visualizer_module");
    if (viz) patch_visualizer(viz);
    sceKernelCpuResumeIntr(irq);
    report("RENDERER embedded runtime started in user memory\n");
}

/* status.txt is a short startup summary: one line per state change (icon,
 * seeking, cassette hook) during the first five minutes, then nothing. */
static int reporter(SceSize args, void *argp)
{
    unsigned i, last_event = 0, last_icon_event = 1, last_seek_event = 0;
    (void)args; (void)argp;
    start_renderer();
    for (i = 0; i < 60; ++i) {
        unsigned current;
        sceKernelDelayThread(5000000);
        /* Stop card logging when USB storage is activated for the host. */
        if (usb_active()) break;
        if (guarded && (i+1)*5 >= TAPE_GUARD_SECONDS) {
            guarded = 0;
            if (!tape_guard_finish()) report("WARNING: could not clear boot-guard.txt\n");
        }
        current = icon_event;
        if (current != last_icon_event && current < sizeof icon_events/sizeof icon_events[0]) {
            report("%s\n", icon_events[current]);
            last_icon_event = current;
        }
        current = seek_event;
        if (current != last_seek_event && current && current < sizeof seek_events/sizeof seek_events[0]) {
            report("%s\n", seek_events[current]);
            last_seek_event = current;
        }
        current = event;
        if (current != last_event && current < sizeof events / sizeof events[0]) {
            report("%s; visualizer=%08X\n", events[current], patched_base);
            last_event = current;
        }
    }
    return sceKernelExitDeleteThread(0);
}


int module_start(SceSize args, void *argp)
{
    SceUID fd, tid;
    unsigned version;
    int model;
    if (sceKernelInitKeyConfig() != PSP_INIT_KEYCONFIG_VSH) return 1;
    if (!tape_paths_init(argp, args) || usb_active()) return 1;
    model = sceKernelGetModel(); /* Diagnostic only; every PSP model can activate. */
    version = sceKernelDevkitVersion();
    if (version != 0x06060010 && version != 0x06060110) {
        report("Viz Tape " VIZ_TAPE_VERSION " SKIPPED: unsupported devkit=%08X model=%d\n", version, model);
        return 1;
    }
    /* Enabled by its plugin-list entry (PLUGINS.TXT on ARK, vsh.txt on PRO). */
    if (!tape_guard_begin()) {
        report("Viz Tape " VIZ_TAPE_VERSION " SKIPPED: the previous start did not finish cleanly "
               "(or boot-guard.txt could not be written). Hooks are off for this "
               "start only; the next start tries again.\n");
        return 1;
    }
    /* A recovery boot appends its skip reason to the failed boot's log.
     * Only an actual activation starts a new log. */
    fd = sceIoOpen(TAPE_LOG_PATH, PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC, 0777);
    if (fd >= 0) sceIoClose(fd);
    guarded = 1;
    report("Viz Tape " VIZ_TAPE_VERSION " started; devkit=%08X model=%d storage=%s\n", version, model, tape_paths.device);
    user_partition.size = sizeof user_partition;
    if (sceKernelQueryMemoryPartitionInfo(PSP_MEMORY_PARTITION_USER, &user_partition) < 0 ||
        !(user_partition.attr & 2) ||
        !tape_set_user_memory(user_partition.startaddr, user_partition.memsize)) {
        report("SKIPPED: user memory partition unavailable or invalid; hooks are off\n");
        if (!usb_active() && tape_guard_finish()) guarded = 0;
        return 1;
    }
    report("MEMORY user partition=%08X size=%08X\n", user_partition.startaddr, user_partition.memsize);
    /* Finish the first-install build before registering hooks or returning
     * to XMB startup, so even the first Music load can select the icon. */
    icon_event = tape_icon_resource_ready() ? 1 : 0;
    if (!icon_event && !usb_active()) {
        if (tape_icon_build()) {
            report("%s\n", icon_events[4]);
            icon_event = 1;
        } else icon_event = 5;
    }
    tid = sceKernelCreateThread("VizTapeStatus", reporter, 0x30, 8192, 0, 0);
    if (tid >= 0 && sceKernelStartThread(tid, 0, 0) < 0) {
        sceKernelDeleteThread(tid);
        tid = -1;
    }
    if (tid < 0) {
        report("Viz Tape " VIZ_TAPE_VERSION " SKIPPED: status thread could not start; hooks are off.\n");
        /* No hook has escaped, so this failed activation can safely unload.
         * If USB owns the card, leave the marker for the next boot instead. */
        if (!usb_active() && tape_guard_finish()) guarded = 0;
        return 1;
    }
    previous_handler = sctrlHENSetStartModuleHandler(on_module_start);
    return 0;
}

/* The chained start handler can remain referenced by later plugins. */
int module_stop(SceSize args, void *argp) { (void)args; (void)argp; return -1; }

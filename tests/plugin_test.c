/* Exercise the production startup/hook/reporter flow without firmware dumps.
 * The icon and guard primitives have separate I/O tests; here their results
 * and scheduling failures are injected at the PSP API boundary. */
#include <assert.h>
#include <stdio.h>
#include "../plugin.c"

static int usb, marker, resource_valid, build_result, guard_result;
static int create_result, start_result, started, deleted, registered;
static int interrupts, redirects, checks, builds, delays, exits, chained;
static int usb_after_check, usb_on_delay, usb_on_start_failure;
static int model, keyconfig, memory_result;
static unsigned devkit, memory_base, memory_size, memory_attr;
static const char *expected_device = "ms0:";
static int (*status_entry)(SceSize, void *);
static STMOD_HANDLER installed;
static char messages[4096];
static size_t message_size;
static int log_truncates, log_syncs;
unsigned char tape_renderer_prx[64] __attribute__((aligned(64))) = {0x7f, 'E', 'L', 'F'};
const unsigned int tape_renderer_prx_size = sizeof tape_renderer_prx;
const unsigned int tape_renderer_buffer_size = 256;
static unsigned char load_buffer[256] __attribute__((aligned(64)));
static int allocation_result, allocation_calls, frees, buffer_unavailable;
static int worker_running, load_result, module_start_result, module_status;
static int load_calls, module_start_calls, unload_calls, runtime_present;
static int viz_present, profile_checks, profile_result, start_auto_unloads, start_callback;
static SceModule runtime_module;
static SceModule visualizer = {
    .nsegment = 2, .text_addr = 0x08900000, .text_size = TAPE_TEXT_SIZE,
    .data_size = TAPE_DATA_SIZE, .bss_size = TAPE_BSS_SIZE,
    .segmentaddr = {0x08900000}, .segmentsize = {TAPE_TEXT_SIZE},
    .modname = "visualizer_module"
};
static SceModule music = {
    .nsegment = 2, .text_addr = 0x08800000, .text_size = TAPE_MUSIC_TEXT_SIZE,
    .data_size = TAPE_MUSIC_DATA_SIZE, .bss_size = TAPE_MUSIC_BSS_SIZE,
    .segmentaddr = {0x08800000}, .segmentsize = {TAPE_MUSIC_TEXT_SIZE},
    .modname = "music_player_module"
};

static int old_handler(SceModule *m) { (void)m; ++chained; return 17; }
int sceUsbGetState(void) { return usb; }
int sceKernelCpuSuspendIntr(void) { assert(!interrupts); interrupts = 1; return 0; }
void sceKernelCpuResumeIntr(int irq) { assert(interrupts && irq == 0); interrupts = 0; }
void sceKernelDcacheWritebackAll(void) { assert(interrupts); }
int sceKernelDelayThread(unsigned us)
{
    assert(us == 5000000 && !interrupts);
    if (++delays == usb_on_delay) usb = PSP_USB_ACTIVATED;
    return 0;
}
int sceKernelExitDeleteThread(int result) { assert(result == 0); ++exits; return 0; }
int sceKernelInitKeyConfig(void) { return keyconfig; }
int sceKernelGetModel(void) { return model; }
unsigned sceKernelDevkitVersion(void) { return devkit; }
int sceKernelQueryMemoryPartitionInfo(int partition, PspSysmemPartitionInfo *info)
{
    assert(partition == PSP_MEMORY_PARTITION_USER && info->size == sizeof *info);
    assert(!registered && !interrupts);
    info->startaddr = memory_base;
    info->memsize = memory_size;
    info->attr = memory_attr;
    return memory_result;
}
SceUID sceKernelCreateThread(const char *name, int (*entry)(SceSize, void *),
                             int priority, int stack, int attr, void *options)
{
    assert(!strcmp(name, "VizTapeStatus") && priority == 0x30 && stack == 8192);
    assert(!attr && !options && !registered);
    status_entry = entry;
    return create_result;
}
int sceKernelStartThread(SceUID tid, SceSize args, void *argp)
{
    assert(tid == create_result && !args && !argp && !registered);
    if (start_result < 0) {
        if (usb_on_start_failure) usb = PSP_USB_ACTIVATED;
        return start_result;
    }
    started = 1;
    return 0;
}
int sceKernelDeleteThread(SceUID tid) { assert(tid == create_result); ++deleted; return 0; }
SceModule *sceKernelFindModuleByName(const char *name)
{
    if (!strcmp(name, TAPE_RENDERER_MODULE)) return runtime_present ? &runtime_module : 0;
    if (!strcmp(name, "visualizer_module")) {
        assert(interrupts); /* Catch-up lookup cannot race a module unload. */
        return viz_present ? &visualizer : 0;
    }
    return 0;
}
SceModule *sceKernelFindModuleByUID(SceUID uid)
{ assert(uid == load_result); return runtime_present ? &runtime_module : 0; }
SceUID sceKernelAllocPartitionMemory(SceUID partition, const char *name, int type, SceSize size, void *address)
{
    assert(worker_running && !interrupts && partition == PSP_MEMORY_PARTITION_USER);
    assert(!strcmp(name, "VizTapeLoad") && type == PSP_SMEM_High && size == sizeof load_buffer && !address);
    ++allocation_calls;
    memset(load_buffer, 0xa5, sizeof load_buffer);
    return allocation_result;
}
void *sceKernelGetBlockHeadAddr(SceUID uid)
{ assert(uid == allocation_result); return buffer_unavailable ? 0 : load_buffer; }
int sceKernelFreePartitionMemory(SceUID uid)
{ assert(uid == allocation_result && !frees); ++frees; return 0; }
/* Model the firmware ABI, independently of the SDK's reversed declaration.
 * The asm alias in module_loader.h binds this to the real import symbol. */
SceUID tape_load_module_buffer(SceSize size, void *buffer, int flags, SceKernelLMOption *options)
{
    assert(worker_running && !interrupts && !renderer_started);
    unsigned i;
    assert(buffer == load_buffer && size == tape_renderer_prx_size && !((uintptr_t)buffer % 64));
    assert(!frees && !memcmp(buffer, tape_renderer_prx, size));
    /* Firmware reads the loaded memory span, not just the PRX file length. */
    for (i = size; i < tape_renderer_buffer_size; ++i) assert(((unsigned char *)buffer)[i] == 0);
    assert(flags == 0 && options->size == sizeof *options);
    assert(options->mpidtext == PSP_MEMORY_PARTITION_USER && options->mpiddata == PSP_MEMORY_PARTITION_USER);
    assert(!options->flags && !options->position && !options->access);
    assert(log_syncs == 1 && strstr(messages, "RENDERER loading embedded runtime"));
    ++load_calls;
    if (load_result >= 0) runtime_present = 1;
    return load_result;
}
int sceKernelStartModule(SceUID uid, SceSize args, void *argp, int *status, void *options)
{
    assert(worker_running && !interrupts && !renderer_started);
    assert(uid == load_result && !args && !argp && !options && runtime_present);
    assert(frees == 1);
    assert(log_syncs == 2 && strstr(messages, "RENDERER starting embedded runtime"));
    ++module_start_calls;
    if (start_callback) {
        assert(installed(&visualizer) == 17);
        assert(!profile_checks && !renderer_started); /* No pointer to unstarted code. */
        viz_present = 1;
    }
    *status = module_status;
    if (start_auto_unloads) runtime_present = 0;
    return module_start_result;
}
int sceKernelUnloadModule(SceUID uid)
{
    assert(worker_running && !interrupts && uid == load_result && runtime_present);
    ++unload_calls;
    runtime_present = 0;
    return 0;
}
uintptr_t sctrlHENFindFunction(const char *module, const char *library, unsigned nid)
{
    /* Private exports must never use a CFW lookup, including fallback after
     * missing/malformed metadata. PAF still needs CFW NID translation. */
    assert(strcmp(module, TAPE_RENDERER_MODULE));
    (void)library; (void)nid; return 0;
}
STMOD_HANDLER sctrlHENSetStartModuleHandler(STMOD_HANDLER handler)
{
    STMOD_HANDLER old = installed;
    assert(started && marker); /* A live guard reporter precedes any hook. */
    installed = handler;
    ++registered;
    return old;
}
SceUID sceIoOpen(const char *path, int flags, int mode)
{
    assert(!interrupts && !usb && !strcmp(path, TAPE_LOG_PATH) && mode == 0777);
    assert(!strncmp(path, expected_device, 4));
    assert(flags == (PSP_O_WRONLY|PSP_O_CREAT|PSP_O_TRUNC) ||
           flags == (PSP_O_WRONLY|PSP_O_CREAT|PSP_O_APPEND));
    if (flags & PSP_O_TRUNC) {
        ++log_truncates;
        message_size = 0;
        messages[0] = 0;
    }
    return 3;
}
int sceIoClose(SceUID fd) { assert(fd == 3); return 0; }
int sceIoSync(const char *device, unsigned flags)
{
    assert(!strcmp(device, expected_device) && !flags && !interrupts && !usb);
    ++log_syncs;
    return 0;
}
int sceIoWrite(SceUID fd, const void *data, unsigned size)
{
    assert(fd == 3 && message_size + size < sizeof messages && !interrupts && !usb);
    memcpy(messages + message_size, data, size);
    message_size += size;
    messages[message_size] = 0;
    return (int)size;
}
int tape_guard_begin(void)
{
    assert(!strncmp(TAPE_GUARD_PATH, expected_device, 4));
    if (guard_result) marker = 1;
    return guard_result;
}
int tape_guard_finish(void) { assert(!usb && !interrupts); marker = 0; return 1; }
int tape_icon_resource_ready(void)
{
    assert(!interrupts && !usb);
    assert(!strncmp(TAPE_ICON_PATH, expected_device, 4));
    ++checks;
    if (usb_after_check) usb = PSP_USB_ACTIVATED;
    return resource_valid;
}
int tape_icon_build(void)
{
    assert(!registered && marker && !usb && !interrupts);
    ++builds;
    resource_valid = build_result;
    return build_result;
}
int tape_icon_redirect(void *text, size_t size, uint32_t base)
{
    assert(interrupts && !usb && resource_valid);
    assert((uintptr_t)text == music.text_addr && size == music.text_size && base == music.text_addr);
    ++redirects;
    return 1;
}
int tape_profile_matches(const void *text, size_t size, uint32_t base)
{ (void)text; (void)size; (void)base; ++profile_checks; return profile_result; }
uint32_t tape_play_state_address(const void *text, size_t size, uint32_t base)
{ (void)text; (void)size; (void)base; return 0; }

static void reset(void)
{
    usb = marker = resource_valid = started = deleted = registered = 0;
    interrupts = redirects = checks = builds = delays = exits = chained = 0;
    usb_after_check = usb_on_delay = usb_on_start_failure = start_result = 0;
    build_result = guard_result = 1;
    create_result = 8;
    status_entry = 0;
    installed = old_handler;
    previous_handler = 0;
    event = patched_base = seek_event = icon_event = guarded = 0;
    message_size = 0; messages[0] = 0;
    log_truncates = log_syncs = 0;
    renderer_started = worker_running = module_status = 0;
    load_calls = module_start_calls = unload_calls = runtime_present = 0;
    viz_present = profile_checks = profile_result = start_auto_unloads = start_callback = 0;
    memset(&runtime_module, 0, sizeof runtime_module);
    load_result = module_start_result = 12;
    allocation_result = 20;
    allocation_calls = frees = buffer_unavailable = 0;
    model = memory_result = 0;
    keyconfig = PSP_INIT_KEYCONFIG_VSH; devkit = 0x06060110;
    memory_base = 0x08800000; memory_size = 0x01800000; memory_attr = 15;
    expected_device = "ms0:";
    assert(tape_paths_init(0, 0));
    assert(tape_set_user_memory(memory_base, memory_size));
}

static void run_worker(void)
{
    worker_running = 1;
    assert(status_entry(0, 0) == 0);
    worker_running = 0;
}

int main(void)
{
    unsigned i;
    /* The real local parser rejects absent/corrupt export metadata without
     * falling back to a firmware lookup or modifying the visualizer. */
    for (i = 0; i < 2; ++i) {
        reset(); renderer_started = runtime_present = profile_result = 1;
        if (i) {
            runtime_module.ent_top = (void *)0x88000000;
            runtime_module.ent_size = sizeof(SceLibraryEntryTable);
        }
        patch_visualizer(&visualizer);
        assert(event == 3 && !patched_base && !interrupts && profile_checks == 1);
    }
    /* Model is diagnostic, never an activation gate. Exercise every retail
     * model value and both the real and ARK-compatibility version values. */
    for (i = 0; i <= 10; ++i) {
        char expected_model[32];
        const char *installed_path = i == 4 ? "ef0:/seplugins/viz_tape/viz_tape.prx" :
                                              "ms0:/seplugins/viz_tape/viz_tape.prx";
        reset(); model = (int)i;
        devkit = i & 1 ? 0x06060110 : 0x06060010;
        if (i) memory_size = 0x03800000; /* Expanded user partition. */
        if (i == 4) expected_device = "ef0:";
        assert(module_start(strlen(installed_path)+1, (void *)installed_path) == 0);
        snprintf(expected_model, sizeof expected_model, "model=%u storage=", i);
        assert(strstr(messages, expected_model));
        assert(tape_user_range(0x0bfffffc, 4) == (i != 0));
        assert(installed(&music) == 17 && redirects == 1);
        run_worker();
        assert(renderer_started && !marker && !guarded && registered == 1);
    }
    /* Go may also install from Memory Stick with the normal 24 MB user
     * partition. Model alone must never grant access to unmapped extra RAM. */
    reset(); model = 4;
    assert(module_start(0, 0) == 0 && !tape_user_range(0x0a000000, 4));
    run_worker(); assert(renderer_started);

    for (i = 0; i < 4; ++i) {
        reset();
        if (i == 0) memory_result = -1;
        if (i == 1) memory_base = 0x88000000;
        if (i == 2) memory_size = 0xffffffff;
        if (i == 3) memory_attr = 0;
        assert(module_start(0, 0) == 1 && !registered && !started && !marker && !guarded);
        assert(strstr(messages, "user memory partition unavailable or invalid"));
    }
    reset(); keyconfig = 1;
    assert(module_start(0, 0) == 1 && !registered && !message_size);
    reset(); devkit = 0x05050010;
    assert(module_start(0, 0) == 1 && !registered && !marker);
    assert(strstr(messages, "unsupported devkit=05050010 model=0"));
    reset();
    assert(module_start(7, "flash0:") == 1 && !registered && !message_size);
    reset(); usb = PSP_USB_ACTIVATED;
    assert(module_start(0, 0) == 1 && !registered && !message_size);
    reset(); usb = -1;
    assert(module_start(0, 0) == 1 && !registered && !message_size);
    /* The very first Music load after startup sees the newly built icon,
     * without running the reporter or waiting five seconds. */
    reset();
    assert(module_start(0, 0) == 0 && builds == 1 && checks == 1 && !delays);
    assert(strstr(messages, "ICON created"));
    assert(installed(&music) == 17 && redirects == 1 && checks == 2 && chained == 1);
    assert(marker && guarded);

    /* A deleted/corrupted file after USB must not inherit boot-time approval.
     * Neither exiting on USB nor the normal five-minute timeout may disable
     * validation. A valid replacement can be picked up on a later load. */
    for (i = 0; i < 2; ++i) {
        reset(); resource_valid = 1;
        assert(module_start(0, 0) == 0 && !builds);
        usb_on_delay = i ? 0 : 1;
        run_worker();
        assert(exits == 1 && renderer_started && load_calls == 1 && module_start_calls == 1);
        assert(delays == (i ? 60 : 1));
        assert(marker == (i ? 0 : 1));
        usb = 0; resource_valid = 0;
        assert(installed(&music) == 17 && redirects == 0 && checks == 2 && !builds);
        resource_valid = 1;
        assert(installed(&music) == 17 && redirects == 1 && checks == 3);
    }

    /* Active/unknown USB state skips even the read. Activation during a
     * checksum read also prevents the RAM redirect. */
    reset(); resource_valid = 1;
    assert(module_start(0, 0) == 0);
    usb = PSP_USB_ACTIVATED;
    assert(installed(&music) == 17 && checks == 1 && !redirects);
    usb = -1;
    assert(installed(&music) == 17 && checks == 1 && !redirects);
    usb = 0; usb_after_check = 1;
    assert(installed(&music) == 17 && checks == 2 && !redirects);
    usb = usb_after_check = 0;
    assert(installed(&music) == 17 && checks == 3 && redirects == 1);

    /* Both thread failure modes leave the previous plugin's handler intact. */
    for (i = 0; i < 2; ++i) {
        reset(); resource_valid = 1;
        if (i) start_result = -1; else create_result = -1;
        assert(module_start(0, 0) == 1 && installed == old_handler && !registered);
        assert(!started && !marker && !guarded && deleted == (int)i);
        assert(!load_calls && !module_start_calls);
        assert(strstr(messages, "status thread could not start; hooks are off"));
    }
    reset(); start_result = -1; usb_on_start_failure = 1;
    assert(module_start(0, 0) == 1 && !registered && marker && guarded);

    /* Build failure preserves stock resource loading; a later valid file
     * still works. Unsupported module layouts never trigger file reads. */
    reset(); build_result = 0;
    assert(module_start(0, 0) == 0 && builds == 1);
    assert(installed(&music) == 17 && !redirects);
    resource_valid = 1;
    assert(installed(&music) == 17 && redirects == 1);
    --music.text_size;
    assert(installed(&music) == 17 && checks == 3 && redirects == 1);
    ++music.text_size;
    assert(installed(0) == 17 && checks == 3);
    reset(); guard_result = 0;
    strcpy(messages, "PREVIOUS BOOT: renderer load did not return\n");
    message_size = strlen(messages);
    assert(module_start(0, 0) == 1 && !registered && !checks && !builds);
    assert(!log_truncates && !log_syncs && !load_calls);
    assert(strstr(messages, "PREVIOUS BOOT: renderer load did not return\n"));
    assert(strstr(messages, "SKIPPED: the previous start did not finish cleanly"));

    /* A normal activation still starts a fresh status summary. */
    reset();
    strcpy(messages, "STALE LOG\n"); message_size = strlen(messages);
    assert(module_start(0, 0) == 0 && log_truncates == 1);
    assert(!strstr(messages, "STALE LOG") && strstr(messages, "Viz Tape " VIZ_TAPE_VERSION " started"));

    /* Renderer startup runs only from the worker. A visualizer that starts
     * before or during that load must be revisited after successful startup. */
    for (i = 0; i < 2; ++i) {
        reset(); resource_valid = 1;
        assert(module_start(0, 0) == 0 && !load_calls && !renderer_started);
        if (i) start_callback = 1;
        else {
            viz_present = 1;
            assert(installed(&visualizer) == 17 && !profile_checks);
        }
        run_worker();
        assert(renderer_started && profile_checks == 1 && !unload_calls);
        assert(allocation_calls == 1 && frees == 1);
        assert(strstr(messages, "embedded runtime started in user memory"));
    }

    /* Load errors, start errors and nonresident starts never enable the draw
     * patch. Unload only if the module manager has not already cleaned up. */
    for (i = 0; i < 4; ++i) {
        reset(); resource_valid = 1; viz_present = 1;
        if (i == 0) load_result = -123;
        else if (i == 1) module_start_result = -456;
        else if (i == 2) { module_start_result = 0; module_status = 1; start_auto_unloads = 1; }
        else module_status = -789;
        assert(module_start(0, 0) == 0);
        run_worker();
        assert(!renderer_started && !runtime_present && !profile_checks);
        assert(load_calls == 1 && module_start_calls == (i ? 1 : 0));
        assert(frees == 1);
        assert(unload_calls == (i == 1 || i == 3));
        assert(strstr(messages, i ? "RENDERER start failed" : "RENDERER load failed"));
        assert(installed(&visualizer) == 17 && !profile_checks);
    }
    reset(); resource_valid = 1; runtime_present = 1;
    assert(module_start(0, 0) == 0);
    run_worker();
    assert(!load_calls && !renderer_started && !unload_calls && runtime_present);
    assert(!allocation_calls);

    for (i = 0; i < 2; ++i) {
        reset(); resource_valid = 1;
        if (i) buffer_unavailable = 1; else allocation_result = -1;
        assert(module_start(0, 0) == 0);
        run_worker();
        assert(!load_calls && !module_start_calls && !renderer_started && !log_syncs);
        assert(allocation_calls == 1 && frees == (int)i);
        assert(strstr(messages, "RENDERER buffer"));
    }

    puts("Plugin lifecycle: immediate icon, per-load validation, USB, thread failures, embedded runtime startup/failure/catch-up and handler chaining passed.");
    return 0;
}

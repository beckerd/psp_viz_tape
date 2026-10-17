#ifndef VIZ_TAPE_PATHS_H
#define VIZ_TAPE_PATHS_H
#include <stddef.h>

typedef struct {
    char device[5];
    char log[sizeof("ms0:/seplugins/viz_tape/status.txt")];
    char guard[sizeof("ms0:/seplugins/viz_tape/boot-guard.txt")];
    char icon[sizeof("ms0:/seplugins/viz_tape/visualizer.rco")];
} TapePaths;
extern TapePaths tape_paths;
#define TAPE_LOG_PATH (tape_paths.log)
#define TAPE_GUARD_PATH (tape_paths.guard)
#define TAPE_ICON_PATH (tape_paths.icon)
/* ARK and PRO pass the installed PRX's full path to module_start. Data stays in
 * the standard viz_tape directory on that same device (Memory Stick/Go).
 * A caller supplying no arguments retains the original ms0: default. */
int tape_paths_init(const void *args, size_t size);
#endif

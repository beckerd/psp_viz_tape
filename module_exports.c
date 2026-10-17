#include "module_exports.h"
#include "safety.h"
#include <string.h>

/* Strings may be byte-aligned. Validate their containing user segment first,
 * then use subtraction so neither a pointer nor its requested extent wraps. */
static int contains_bytes(const SceModule *module, uintptr_t address, size_t size)
{
    unsigned i;
    if (!module || module->nsegment < 1 || module->nsegment > 4 || !size) return 0;
    for (i = 0; i < module->nsegment; ++i) {
        uintptr_t base = module->segmentaddr[i];
        size_t span = module->segmentsize[i];
        if (tape_user_range(base, span) && address >= base && address-base < span &&
            size <= span-(address-base)) return 1;
    }
    return 0;
}

uintptr_t tape_find_export(const SceModule *module, const char *library,
                           uint32_t nid, int variable)
{
    uintptr_t start;
    size_t offset = 0, name_size;
    if (!module || !library || (variable != 0 && variable != 1)) return 0;
    start = (uintptr_t)module->ent_top;
    if (!start || (start & 3) || module->ent_size < sizeof(SceLibraryEntryTable) ||
        module->ent_size > 0x4000 || (module->ent_size & 3) ||
        !contains_bytes(module, start, module->ent_size)) return 0;
    name_size = strlen(library) + 1;
    while (offset < module->ent_size) {
        SceLibraryEntryTable entry;
        size_t remaining = module->ent_size-offset, size;
        unsigned count, first, last, i;
        const uint32_t *table;
        if (remaining < sizeof entry) return 0;
        memcpy(&entry, (const void *)(start+offset), sizeof entry);
        size = (unsigned)entry.len * 4u;
        if (size < sizeof entry || size > remaining) return 0;
        /* A null name is the system-export library. Comparing the entire
         * expected name including NUL never scans beyond a verified range. */
        if (entry.libname && contains_bytes(module, (uintptr_t)entry.libname, name_size) &&
            !memcmp(entry.libname, library, name_size)) {
            count = (unsigned)entry.stubcount + entry.vstubcount;
            table = entry.entrytable;
            /* This is only for our small private library, not arbitrary
             * firmware export sets. Counts bound all indexing and work. */
            if (!count || count > 64 || ((uintptr_t)table & 3) ||
                !contains_bytes(module, (uintptr_t)table, count * 8u)) return 0;
            first = variable ? entry.stubcount : 0;
            last = variable ? count : entry.stubcount;
            for (i = first; i < last; ++i)
                if (table[i] == nid) return table[count+i];
            return 0;
        }
        offset += size;
    }
    return 0;
}

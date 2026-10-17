/* Host allocations model readable user segments; production user-address
 * limits are exercised independently by safety_test. ASan guards each region. */
#include "../module_exports.h"
#include "../runtime.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DRAW_NID 0xeab11153u
#define BRIDGE_NID 0x99f748f8u
#define DRAW_ADDRESS 0x08910000u
#define BRIDGE_ADDRESS 0x08920000u
static SceModule module;
static SceLibraryEntryTable *entries;
static char *name;
static uint32_t *table;
static uintptr_t readable[3];
static size_t spans[3];

int tape_user_range(uintptr_t address, size_t size)
{
    unsigned i;
    if ((address & 3) || !size) return 0;
    for (i = 0; i < 3; ++i)
        if (address >= readable[i] && address-readable[i] < spans[i] &&
            size <= spans[i]-(address-readable[i])) return 1;
    return 0;
}

static void fixture(void)
{
    unsigned i;
    free(entries); free(name); free(table);
    memset(&module, 0, sizeof module);
    spans[0] = 3 * sizeof *entries;
    spans[1] = sizeof(TAPE_RENDERER_LIBRARY) + 1;
    spans[2] = 6 * sizeof *table;
    entries = calloc(1, spans[0]); name = malloc(spans[1]); table = malloc(spans[2]);
    assert(entries && name && table);
    readable[0] = (uintptr_t)entries;
    readable[1] = (uintptr_t)name;
    readable[2] = (uintptr_t)table;
    for (i = 0; i < 3; ++i) {
        module.segmentaddr[i] = readable[i];
        module.segmentsize[i] = spans[i];
    }
    module.nsegment = 3;
    module.ent_top = entries;
    module.ent_size = 2 * sizeof *entries;
    entries[0].len = entries[1].len = sizeof *entries / 4;
    /* The system library has a null name; the private name is unaligned. */
    strcpy(name+1, TAPE_RENDERER_LIBRARY);
    entries[1].libname = name+1;
    entries[1].stubcount = 2; entries[1].vstubcount = 1;
    entries[1].entrytable = table;
    table[0] = 0x12345678; table[1] = DRAW_NID; table[2] = BRIDGE_NID;
    table[3] = 0x08900000; table[4] = DRAW_ADDRESS; table[5] = BRIDGE_ADDRESS;
}

static uintptr_t find(uint32_t nid, int variable)
{ return tape_find_export(&module, TAPE_RENDERER_LIBRARY, nid, variable); }

static void rejected(void)
{ assert(!find(DRAW_NID, 0) && !find(BRIDGE_NID, 1)); }

int main(void)
{
    unsigned i;
    fixture();
    assert(find(DRAW_NID, 0) == DRAW_ADDRESS);
    assert(find(BRIDGE_NID, 1) == BRIDGE_ADDRESS);
    assert(!find(BRIDGE_NID, 0) && !find(DRAW_NID, 1));
    assert(!find(0xdeadbeef, 1) && !find(DRAW_NID, 2));
    assert(!tape_find_export(0, TAPE_RENDERER_LIBRARY, DRAW_NID, 0));
    assert(!tape_find_export(&module, 0, DRAW_NID, 0));
    assert(!tape_find_export(&module, "VizTapeRuntimeExtra", DRAW_NID, 0));
    assert(!tape_find_export(&module, "VizTape", DRAW_NID, 0));

    /* A function NID must never stand in for a variable of the same name. */
    table[0] = BRIDGE_NID;
    assert(find(BRIDGE_NID, 1) == BRIDGE_ADDRESS);
    assert(find(BRIDGE_NID, 0) == 0x08900000);
    entries[1].vstubcount = 0;
    assert(!find(BRIDGE_NID, 1));

    /* Len is in 32-bit words, not sizeof(header) records. A longer header
     * can leave the next native-width host header only 4-byte aligned. */
    fixture();
    {
        SceLibraryEntryTable private_entry = entries[1];
        size_t next = sizeof *entries + 4;
        memset(entries+1, 0, sizeof *entries+4);
        entries[0].len += 1;
        memcpy((char *)entries+next, &private_entry, sizeof private_entry);
        module.ent_size += 4;
        assert(find(DRAW_NID, 0) == DRAW_ADDRESS && find(BRIDGE_NID, 1) == BRIDGE_ADDRESS);
    }

    for (i = 0; i < 10; ++i) {
        fixture();
        switch (i) {
        case 0: module.ent_top = 0; break;
        case 1: module.ent_top = (char *)entries+1; break;
        case 2: module.ent_size = 0; break;
        case 3: module.ent_size = sizeof *entries-4; break;
        case 4: module.ent_size -= 4; break;
        case 5: module.ent_size = 0x4004; break;
        case 6: entries[0].len = 0; break;
        case 7: entries[0].len = sizeof *entries/4-1; break;
        case 8: entries[0].len = 255; break;
        case 9: module.ent_top = (void *)(UINTPTR_MAX-3); break;
        }
        rejected();
    }

    /* A short/unterminated name at the actual allocation boundary must not
     * reach strcmp or memcmp without validating the entire compared range. */
    fixture();
    entries[1].libname = name+spans[1]-4;
    memcpy((void *)entries[1].libname, "VizT", 4);
    rejected();
    fixture(); name[spans[1]-1] = 'X'; rejected();
    fixture(); entries[1].libname = (char *)0x88000000; rejected();
    fixture(); entries[1].libname = 0; rejected();

    for (i = 0; i < 10; ++i) {
        fixture();
        switch (i) {
        case 0: entries[1].stubcount = entries[1].vstubcount = 0; break;
        case 1: entries[1].stubcount = 64; break;
        case 2: entries[1].stubcount = 65535; entries[1].vstubcount = 255; break;
        case 3: entries[1].entrytable = 0; break;
        case 4: entries[1].entrytable = (char *)table+1; break;
        case 5: entries[1].entrytable = table+1; break; /* NIDs fit, addresses do not. */
        case 6: module.nsegment = 0; break;
        case 7: module.nsegment = 5; break;
        case 8: module.nsegment = 2; break; /* Table belongs to another module. */
        case 9: module.segmentsize[0] = 0xffffffff; break;
        }
        rejected();
    }
    free(entries); free(name); free(table);
    puts("Exports: typed function/variable lookup, variable-length headers, bounded names/tables, ownership and malformed metadata rejection passed.");
    return 0;
}

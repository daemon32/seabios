// Support for building memory maps suitable for int 15 e820 calls.
//
// Copyright (C) 2008  Kevin O'Connor <kevin@koconnor.net>
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "memmap.h" // struct e820entry
#include "util.h" // dprintf.h
#include "biosvar.h" // SET_EBDA

// Remove an entry from the e820_list.
static void
remove_e820(int i)
{
    e820_count--;
    memmove(&e820_list[i], &e820_list[i+1]
            , sizeof(e820_list[0]) * (e820_count - i));
}

// Insert an entry in the e820_list at the given position.
static void
insert_e820(int i, u64 start, u64 size, u32 type)
{
    if (e820_count >= CONFIG_MAX_E820) {
        dprintf(1, "Overflowed e820 list!\n");
        return;
    }

    memmove(&e820_list[i+1], &e820_list[i]
            , sizeof(e820_list[0]) * (e820_count - i));
    e820_count++;
    struct e820entry *e = &e820_list[i];
    e->start = start;
    e->size = size;
    e->type = type;
}

// Show the current e820_list.
static void
dump_map()
{
    dprintf(1, "e820 map has %d items:\n", e820_count);
    int i;
    for (i=0; i<e820_count; i++) {
        struct e820entry *e = &e820_list[i];
        u64 e_end = e->start + e->size;
        dprintf(1, "  %d: %x%x - %x%x = %d\n", i
                , (u32)(e->start >> 32), (u32)e->start
                , (u32)(e_end >> 32), (u32)e_end
                , e->type);
    }
}

// Add a new entry to the list.  This scans for overlaps and keeps the
// list sorted.
void
add_e820(u64 start, u64 size, u32 type)
{
    dprintf(8, "Add to e820 map: %x %x %d\n", (u32)start, (u32)size, type);

    if (! size)
        // Huh?  Nothing to do.
        return;

    u64 end = start + size;
    int i;
    for (i=0; i<e820_count; i++) {
        struct e820entry *e = &e820_list[i];
        if (end < e->start)
            // Simple insertion point.
            break;
        u64 e_end = e->start + e->size;
        if (start > e_end)
            // No overlap.
            continue;
        // New item overlaps (or borders) an existing one.
        if (start > e->start) {
            e->size = start - e->start;
            i++;
            if (end < e_end)
                // Need to split existing item
                insert_e820(i, end, e_end - end, e->type);
            if (type == e->type) {
                // Same type - merge them.
                size += start - e->start;
                start = e->start;
                i--;
                remove_e820(i);
            }
        }
        if (type != E820_HOLE) {
            insert_e820(i, start, size, type);
            i++;
        }
        // Remove all existing items that are completely overlapped.
        while (i<e820_count) {
            e = &e820_list[i];
            if (end < e->start)
                // No overlap - done.
                break;
            e_end = e->start + e->size;
            if (end >= e_end) {
                // Existing item completely overlapped - remove it.
                remove_e820(i);
                continue;
            }
            // Not completely overlapped - adjust its start.
            e->start = end;
            e->size = e_end - e->start;
            if (type == e->type) {
                // Same type - merge them.
                (e-1)->size += e->size;
                remove_e820(i);
            }
            break;
        }
        //dump_map();
        return;
    }
    // Just insert item.
    insert_e820(i, start, size, type);
    //dump_map();
}

// Prep for memmap stuff - init bios table locations.
void
memmap_setup()
{
    bios_table_cur_addr = (u32)BiosTableSpace;
    bios_table_end_addr = bios_table_cur_addr + CONFIG_MAX_BIOSTABLE;
    dprintf(1, "bios_table_addr: 0x%08x end=0x%08x\n",
            bios_table_cur_addr, bios_table_end_addr);

    e820_count = 0;
}

// Report on final memory locations.
void
memmap_finalize()
{
    dump_map();

    dprintf(1, "final bios_table_addr: 0x%08x (used %d%%)\n"
            , bios_table_cur_addr
            , (100 * (bios_table_cur_addr - (u32)&BiosTableSpace)
               / CONFIG_MAX_BIOSTABLE));
    if (bios_table_cur_addr > bios_table_end_addr)
        panic("bios_table_end_addr overflow!\n");
}

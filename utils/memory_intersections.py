#!/bin/python

"""
The script intended to find intersections in rows of memory map
description entries for two targets which is stored in two files
passed here as arguments

Script will look at physical addresses of mem regions only

The script is format sensitive if entry format will be different from:
1 * 4K page(s) 0x0000000fe000000 -> 0x000121b8f000 RW- RW- Normal <SHARED_OWNED>
the script will behave unpredictably
"""

import sys

if len(sys.argv) != 3:
    print("Usage: mem_intersect.py [s2#1.log] [s2#2.log]\n" +  \
          "Pass two arguments with s2 mappings entries")
    sys.exit()

def H2Msize(hreadable: str) -> int:
    match hreadable:
        case "4K":
            return 4096
        case "2M":
            return 2097152
        case "1G":
            return 1073741824
        case _:
            print("Unrecognized human readable size of a region")
            return -1

""" Case 1 - no intersections
    ----------------- phys_start 1
    ----------------- phys_end   1
    ================= phys_start 2
    ================= phys_end   2

    Case 2 - no intersections
    ================= phys_start 2
    ================= phys_end   2
    ----------------- phys_start 1
    ----------------- phys_end   1

    Other cases points to intersections in memory regions
"""
def find_intersections(fmap, smap):
    global intersected_size_non_shared
    global intersected_size
    for fen in fmap:
        for sen in smap:
            f_phys_start = int(fmap[fen]["phys_start"], 16)
            f_phys_end = int(fmap[fen]["phys_end"], 16)
            s_phys_start = int(smap[sen]["phys_start"], 16)
            s_phys_end = int(smap[sen]["phys_end"], 16)

            #if (fmap[fen]["phys_start"] < smap[sen]["phys_start"] and
            #    fmap[fen]["phys_end"] < smap[sen]["phys_start"]):
            #    pass
            #elif (fmap[fen]["phys_start"] > smap[sen]["phys_end"] and
            #      fmap[fen]["phys_end"] > smap[sen]["phys_end"]):
            #    pass
            if (f_phys_start < s_phys_start and
                f_phys_end < s_phys_start):
                pass
            elif(f_phys_start > s_phys_end and
                 f_phys_end > s_phys_end):
                pass
            else:
                print(fmap[fen])
                print(smap[sen])
                addr_list = [fmap[fen]["phys_start"],
                             fmap[fen]["phys_end"],
                             smap[sen]["phys_start"],
                             smap[sen]["phys_end"],
                             ]
                addr_list.sort()

                # The hack is: if sort starts and ends ascendingly
                # of two mem regions (4 items) intersection size
                # will be difference between third and second items
                inter_size_local = int(addr_list[2], 16) - int(addr_list[1], 16)
                intersected_size += inter_size_local + 1
                print("Intersected size is - {}".format(inter_size_local + 1))
                if ("page_state" not in fmap[fen] and
                    "page_state" not in smap[sen]):
                    intersected_size_non_shared += inter_size_local + 1
                print('------')

def create_mem_dict(filename) -> dict:
    with open(filename, 'r') as file:
        memmap = dict()
        i = 1
        for line in file:
            linelist = line.strip().split(" ")
            # SCRIPT LOGIC IS FORMAT SENSITIVE
            if len(linelist) != 10 and len(linelist) != 11:
                continue

            memmap[i] = dict()

            size = int(linelist[0]) * H2Msize(linelist[2])
            virt_start, phys_start = int(linelist[4], 16), int(linelist[6], 16)
            memmap[i]["size"]       = size
            memmap[i]["virt_start"] = hex(virt_start)
            memmap[i]["virt_end"]   = hex(virt_start + size - 1)
            memmap[i]["phys_start"] = hex(phys_start)
            memmap[i]["phys_end"]   = hex(phys_start + size - 1)

            if linelist[-1].startswith('SHARED'):
                memmap[i]["page_state"] = linelist[-1]

            i += 1
    return memmap

intersected_size_non_shared = 0
intersected_size = 0
first_map = create_mem_dict(sys.argv[1])
second_map = create_mem_dict(sys.argv[2])
find_intersections(first_map, second_map)

print("Total intersected size: ")
print("{} bytes for non-shared regions;".format(intersected_size_non_shared))
print("{} bytes for case where one or both regions have any shared page status;".format(intersected_size))

del first_map
del second_map

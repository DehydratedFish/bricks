#pragma once

#include "definitions.h"
#include "list.h"


struct Blueprint;


struct BrickyardEntry {
    String name;
    String version;
    String path;
};

// TODO: Change to a hash table, but then it needs to be able to store multiple
//       entries with the same name.
//       Also add groups so you can be more specific about how you want to store libraries.
struct Brickyard {
    Allocator allocator;

    // TODO: This will still be allocated by the DefaultAllocator.
    List<BrickyardEntry> entries;
    b32 is_dirty;
};


b32 load_brickyard(Brickyard *yard, String file, Allocator alloc);
b32 save_brickyard(Brickyard *yard, String file, b32 force = false);

void destroy(Brickyard *yard);

void   add (Brickyard *yard, String name, String version, String path);
String find(Brickyard *yard, String name, String version = "");


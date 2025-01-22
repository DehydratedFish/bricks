#include "brickyard.h"

#include "platform.h"
#include "binary.h"


enum EntryKind : u8 {
    ENTRY_BLUEPRINT = 0x01,
    // ENTRY_GROUP     = 0x02,
};

b32 load_brickyard(Brickyard *yard, String file, Allocator alloc) {
    auto read_result = platform_read_entire_file(file);
    DEFER(destroy(&read_result.content));

    if (read_result.error) return false;
    String content = read_result.content;

    if (yard->allocator.allocate) destroy(yard);
    yard->allocator = alloc;

    s64 offset = 0x00;
    while (offset < content.size) {
        EntryKind kind = (EntryKind)read_byte(content, &offset);

        if (kind == ENTRY_BLUEPRINT) {
            String name    = read_binary_string(content, &offset);
            String version = read_binary_string(content, &offset);
            String path    = read_binary_string(content, &offset);

            add(yard, name, version, path);
        } else {
            destroy(yard);
            return false;
        }
    }

    return true;
}

b32 save_brickyard(Brickyard *yard, String file, b32 force) {
    if (yard->is_dirty || force) {
        PlatformFile yard_file = platform_file_open(file, PlatformFileOverride);
        if (!yard_file.open) return false;

        StringBuilder builder = {};
        DEFER(destroy(&builder));

        FOR (yard->entries, entry) {
            write_binary(&builder, ENTRY_BLUEPRINT);
            write_binary_string(&builder, entry->name);
            write_binary_string(&builder, entry->version);
            write_binary_string(&builder, entry->path);
        }

        if (!write_builder_to_file(&builder, &yard_file)) return false;

    }

    return true;
}

void destroy(Brickyard *yard) {
    FOR (yard->entries, entry) {
        destroy(&entry->name);
        destroy(&entry->version);
        destroy(&entry->path);
    }

    destroy(&yard->entries);
    INIT_STRUCT(yard);
}

void add(Brickyard *yard, String name, String version, String path) {
    assert(yard->allocator.allocate);

    BrickyardEntry entry = {};
    entry.name    = allocate_string(name,    yard->allocator);
    entry.version = allocate_string(version, yard->allocator);
    entry.path    = allocate_string(path,    yard->allocator);

    append(&yard->entries, entry);

    yard->is_dirty = true;
}

String find(Brickyard *yard, String name, String version) {
    FOR (yard->entries, entry) {
        // TODO: Use the latest entry if no version is specified and
        //       no empty version is found.
        if (entry->name == name && entry->version == version) {
            return entry->path;
        }
    }

    return "";
}


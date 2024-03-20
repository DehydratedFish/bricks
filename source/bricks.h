#pragma once

#include "definitions.h"

typedef b32 CompilerBuildFunc(Allocator alloc, struct Blueprint *blueprint, struct Sheet *sheet);

struct Compiler {
    String name;

    CompilerBuildFunc *build;
};

enum PlatformKind {
    TARGET_UNDEFINED,
    TARGET_WIN32,

    TARGET_COUNT,
};

struct TargetPlatformInfo {
    String exe;
    String static_lib;
    String shared_lib;
};

struct ApplicationState {
    // TODO: make a block based arena that can grow in size
    MemoryArena string_storage;
    Allocator   string_alloc;

    String starting_directory;
    String build_files_directory;

    String config_directory;
    String brickyard_directory;

    String blueprint_file;
    String cache_file;

    PlatformKind target_platform;
    TargetPlatformInfo target_info;

    String build_type;
    String group;

    b32 verbose;
};


struct Dependency {
    String module;
    String sheet;
};

enum {
    SHEET_STATIC,
    SHEET_SHARED,
};

enum SheetKind {
    SHEET_NONE,
    SHEET_BLUEPRINT,
    SHEET_BRICK,
    SHEET_EXECUTABLE,
    SHEET_LIBRARY,

    SHEET_COUNT,
};
enum SheetStatus {
    SHEET_STATUS_UNBUILD,
    SHEET_STATUS_READY,
    SHEET_STATUS_ERROR,
};
struct Sheet {
    SheetKind kind;
    u32 additional_info;

    SheetStatus status;

    String name;
    String full_path;

    String compiler;
    String linker;

    String group;

    String build_dir;

    DArray<String> include_dirs;
    DArray<String> symbols;
    DArray<String> sources;
    DArray<String> libraries;

    DArray<Dependency> dependencies;

    DArray<String> diagnostics;
};

struct Blueprint {
    String name;

    String file;
    String path;

    String source;

    String compiler;
    String linker;

    String build_dir;

    DArray<Sheet> sheets;
    DArray<Blueprint> blueprints;

    bool valid;
};


// NOTE: declarations to silence clangd
INTERNAL String convert_unsigned_to_string(u8 *buffer, s32 buffer_size, u64 number, s32 base = 10, b32 uppercase = true);
INTERNAL String convert_signed_to_string(u8 *buffer, s32 buffer_size, s64 signed_number, s32 base = 10, b32 uppercase = true, b32 keep_sign = false);

s64 print(String fmt, ...);
String format(Allocator alloc, String fmt, ...);
String format(String *fmt, ...);
String t_format(String fmt, ...);

b32 flush_write_buffer(struct PlatformFile *file);

Blueprint parse_blueprint_file(String file);
void free_blueprint(Blueprint *blueprint);

String to_utf8(Allocator alloc, String16 string);

String16 to_utf16(MemoryArena *arena, String string, b32 add_null_terminator = false);
String16 to_utf16(MemoryArena arena, String string, b32 add_null_terminator = false);

String16 to_utf16(Allocator alloc, String string, b32 add_null_terminator = false);

UTF8Iterator make_utf8_it(String string);
void next(UTF8Iterator *it);

UTF8CharResult utf8_peek(String str);

INTERNAL void *allocate(Allocator allocator, s64 bytes);
INTERNAL void *reallocate(Allocator allocator, s64 bytes, void *old, s64 old_bytes);
INTERNAL void deallocate(Allocator allocator, void *ptr, s64 old_size);

#define ALLOC(alloc, type, count) (type*)allocate(alloc, (count) * sizeof(type))
#define REALLOC(alloc, ptr, old_count, new_count) (decltype(ptr))reallocate(alloc, (new_count) * sizeof(*ptr), ptr, (old_count) * sizeof(*ptr))
#define DEALLOC(alloc, ptr, count) deallocate(alloc, ptr, (count) * sizeof(*ptr))

struct ApplicationToolbox {
    Allocator default_allocator;

    MemoryArena temporary_storage;
};

ApplicationToolbox *get_toolbox();
Allocator default_allocator();
Allocator change_default_allocator(Allocator alloc);

Allocator temporary_allocator();
s64  temporary_storage_mark();
void temporary_storage_rewind(s64 mark);
void reset_temporary_storage();

template<typename Type> INTERNAL Array<Type> allocate_array(s64 size);
template<typename Type> INTERNAL void destroy_array(Array<Type> *array);
#define ALLOCATE_ARRAY(type, size) allocate_array<type>(size)

template<typename Type> INTERNAL Type *append(DArray<Type> &array, Type element);
template<typename Type> INTERNAL void destroy(DArray<Type> &array);

INTERNAL void zero_memory(void *data, s64 size);
INTERNAL void copy_memory(void *dest, void const *src, s64 size);

#define INIT_STRUCT(ptr) zero_memory(ptr, sizeof(*ptr))

INTERNAL String allocate_string(Allocator alloc, s64 size);
INTERNAL String allocate_string(s64 size);
INTERNAL String allocate_string(Allocator alloc, String str);
INTERNAL String allocate_string(String str);
INTERNAL void destroy_string(String *str);
INTERNAL String sub_string(String buffer, s64 offset, s64 size);
INTERNAL String path_without_filename(String path);
INTERNAL bool ends_with(String str, String search);
INTERNAL bool equal(String lhs, String rhs);
INTERNAL bool operator==(String lhs, String rhs);
INTERNAL bool operator!=(String lhs, String rhs);

INTERNAL void destroy(StringBuilder *builder);
INTERNAL void reset(StringBuilder *builder);
INTERNAL void append(StringBuilder *builder, u8 c);
INTERNAL void append(StringBuilder *builder, String str);
INTERNAL void append_format(StringBuilder *builder, char const *fmt, ...);
INTERNAL String to_allocated_string(StringBuilder *builder, Allocator alloc = default_allocator());

INTERNAL MemoryArena allocate_arena(s64 size);
INTERNAL void destroy(MemoryArena *arena);
INTERNAL Allocator make_arena_allocator(MemoryArena *arena);


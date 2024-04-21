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


struct BrickyardEntry {
    String name;
    String path;
};
struct Brickyard {
    DArray<BrickyardEntry> entries;
    String content;
    b32 loaded;
};


struct ApplicationState {
    // TODO: make a block based arena that can grow in size
    MemoryArena string_storage;
    Allocator   string_alloc;

    String starting_directory;
    String build_files_directory;

    String config_directory;
    String brickyard_file;
    Brickyard brickyard;

    String blueprint_file;

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

enum LoadResult {
    LOAD_OK,
    LOAD_PARSE_ERROR,
};


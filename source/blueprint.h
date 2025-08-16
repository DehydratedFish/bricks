#pragma once

#include "bricks.h"
#include "list.h"


struct StringBuilder;

struct Dependency {
    String module;
    String entity;
};

enum LibraryKind {
    NO_LIBRARY,
    STATIC_LIBRARY,
    SHARED_LIBRARY,
};

const s32 ENTITY_COMMAND_COUNT = 4;

enum EntityKind {
    ENTITY_NONE,
    ENTITY_BRICK,
    ENTITY_EXECUTABLE,
    ENTITY_LIBRARY,

    ENTITY_COUNT,
};
enum EntityStatus {
    ENTITY_STATUS_UNBUILD,
    ENTITY_STATUS_READY,
    ENTITY_STATUS_ERROR,
};
struct Entity {
    EntityKind kind;
    EntityStatus status;

    LibraryKind lib_kind;

    String file_path;
    String intermediate_folder;

    String compiler;
    String linker;

    String name;
    String build_folder;

    List<String> include_folders;
    List<String> symbols;
    List<String> sources;
    List<String> libraries;
    List<String> groups;

    List<Dependency> dependencies;
    
    List<String> compiler_flags;


    // NOTE: Needed to write out the commands into a shell script.
    //       Multiple commands needed for libs as e.g. msvc can't link libs in one go
    //       and needs seperate object files to be linked.
    String build_commands[ENTITY_COMMAND_COUNT];
    s32 build_command_count;

    List<Diagnostic> diagnostics;
    b32 has_errors;
};

enum BlueprintStatus {
    BLUEPRINT_INIT,
    BLUEPRINT_PARSING,
    BLUEPRINT_BUILDING,
    BLUEPRINT_READY,
    BLUEPRINT_ERROR,
};
struct Blueprint {
    BlueprintStatus status;
    String name;

    String file;
    String path;

    String compiler;
    String linker;

    String build_folder;
    String build_type;

    List<Entity>    entitys;
    List<Blueprint> imports;
};


void parse_blueprint_file(Blueprint *bp, String file);
void destroy(Blueprint *bp);

Blueprint *find_submodule (Blueprint *bp, String name);
Entity    *find_dependency(Blueprint *bp, String name);

void add_build_command(Entity *entity, StringBuilder *builder);

void print_diagnostics(Entity *entity);
void add_diagnostic(Entity *entity, DiagnosticKind kind, String msg);


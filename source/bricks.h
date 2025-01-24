#pragma once

#include "definitions.h"
#include "arena.h"
#include "string2.h"
#include "brickyard.h"


struct Blueprint;
struct Entity;

typedef void CompilerBuildFunc(Allocator alloc, Blueprint *blueprint, Entity *entity);
struct Compiler {
    String name;

    CompilerBuildFunc *build;
};


struct TargetPlatformInfo {
    String exe;
    String static_lib;
    String shared_lib;
};

enum DiagnosticKind {
    DIAG_GENERAL,
    DIAG_NOTE,
    DIAG_WARNING,
    DIAG_ERROR,
};
struct Diagnostic {
    DiagnosticKind kind;
    String message;
};


// TODO: The state could contain an anonymous or root Entity.
//       This maybe simplyfies error handling a bit.
//       Also the parser could be a bit smaller.
struct ApplicationState {
    // TODO: The allocator should grow in size.
    MemoryArena string_storage;
    Allocator   string_alloc;

    String starting_folder;
    String build_files_folder;

    String config_folder;
    String brickyard_file;
    Brickyard brickyard;

    String target_platform;
    TargetPlatformInfo target_info;

    String build_type;
    String group;

    List<Diagnostic> diagnostics;
    b32 has_errors;

    List<Compiler> compilers;

    b32 verbose;
};


void add_diagnostic(DiagnosticKind kind, String msg);

b32 be_verbose();

void load_core_compilers();
b32  load_compiler_plugin(String shared_lib);


// TODO: This should not be here.
inline String remove_leading_slashes(String str) {
    while (str.size) {
        if (str[1] == '/' || str[1] == '\\') {
            str = shrink_front(str, 1);
        } else {
            break;
        }
    }

    return str;
}

inline String remove_trailing_slashes(String str) {
    while (str.size) {
        if (str[-1] == '/' || str[-1] == '\\') {
            str = shrink_back(str, 1);
        } else {
            break;
        }
    }

    return str;
}

inline String path_without_filename(String path) {
    String result = {path.data, 0};

    for (s64 i = path.size; i > 0; i -= 1) {
        s64 index = i - 1;
        if (path[index] == '/' || path[index] == '\\') {
            result.size = index;
            break;
        }
    }

    return result;
}


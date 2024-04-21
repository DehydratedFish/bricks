#include "bricks.h"
#include "platform.h"
#include "memory.cpp"
#include "string2.cpp"
#include "io.cpp"
#include "blueprint.cpp"


ApplicationState App;

INTERNAL b32 msvc_build_command(Allocator alloc, Blueprint *blueprint, Sheet *sheet);

INTERNAL Compiler MSVC_Compiler = {
    "msvc",

    msvc_build_command,
};

INTERNAL void merge_arrays(DArray<String> *dest, Array<String> src) {
    FOR (src, string) {
        append_unique(*dest, *string);
    }
}

INTERNAL void add_brick_to_sheet(Sheet *sheet, Sheet *brick) {
    assert(brick->kind == SHEET_BRICK);

    merge_arrays(&sheet->include_dirs, brick->include_dirs);
    merge_arrays(&sheet->sources,      brick->sources);
    merge_arrays(&sheet->libraries,    brick->libraries);
    merge_arrays(&sheet->symbols,      brick->symbols);
}

INTERNAL Sheet *find_dependency(Blueprint *blueprint, Dependency dep) {
    Blueprint *module = blueprint;

    if (dep.module.size) {
        FOR (blueprint->blueprints, bp) {
            if (bp->name == dep.module) {
                module = bp;

                break;
            }
        }

        if (module == blueprint) {
            // TODO: log error in Sheet
            print("Could not find blueprint %S for dependency %S.%S\n", dep.module, dep.module, dep.sheet);

            return 0;
        }
    }

    FOR (module->sheets, sheet) {
        if (sheet->name == dep.sheet) {
            return sheet;
        }
    }

    // TODO: log error in Sheet
    print("Could not find dependency %S.\n", dep.sheet);

    return 0;
}

INTERNAL String remove_line(String *str) {
    String result;
    result.data = str->data;
    
    u32 const multi_char_line_end = '\n' + '\r';
    for (s64 i = 0; i < str->size; i += 1) {
        if (str->data[i] == '\n' || str->data[i] == '\r') {
            result.size = i;
            if (i + 1 < str->size && str->data[i] + str->data[i + 1] == multi_char_line_end) i += 1;

            str->data += i + 1;
            str->size -= i + 1;

            return result;
        }
    }

    result = *str;
    *str = {};

    return result;
}

INTERNAL b32 process_diagnostics(String output, DArray<String> *diagnostics) {
    b32 encountered_errors = false;

    while (output.size) {
        String line = remove_line(&output);

        if (contains(line, ": error ")) {
            append(*diagnostics, allocate_string(App.string_alloc, line));
            encountered_errors = true;
        } else if (contains(line, ": fatal error ")) {
            append(*diagnostics, allocate_string(App.string_alloc, line));
            encountered_errors = true;
        } else if (contains(line, " Command line error ")) {
            append(*diagnostics, allocate_string(App.string_alloc, line));
            encountered_errors = true;
        } else if (contains(line, ": warning")) {
            append(*diagnostics, allocate_string(App.string_alloc, line));
        } else if (contains(line, ": note: ")) {
            append(*diagnostics, allocate_string(App.string_alloc, line));
        }
    }

    return encountered_errors;
}

INTERNAL b32 msvc_build_command(Allocator alloc, Blueprint *blueprint, Sheet *sheet) {
    RESET_TEMP_STORAGE_ON_EXIT();
    CHANGE_AND_RESET_DEFAULT_ALLOCATOR(alloc);

    if (sheet->status == SHEET_STATUS_READY) return true;
    if (sheet->status == SHEET_STATUS_ERROR) return false;

    FOR (sheet->dependencies, dep) {
        Sheet *sub = find_dependency(blueprint, *dep);
        if (!sub) {
            append(sheet->diagnostics, format(App.string_alloc, "Could not build Sheet %S.", sheet->name));

            return false;
        }

        switch (sub->kind) {
        case SHEET_BRICK: {
            add_brick_to_sheet(sheet, sub);
        } break;

        case SHEET_LIBRARY: {
            if (msvc_build_command(alloc, blueprint, sub)) {
                append(sheet->libraries, sub->full_path);
            } else {
                FOR (sub->diagnostics, diag) {
                    print("%S\n", diag);
                }

                return false;
            }
        } break;

        default:
            append(sheet->diagnostics, format(App.string_alloc, "Can only add Bricks and Libraries as dependencies at the moment. (sheet: %S, dep: %S)\n", sheet->name, sub->name));

            return false;
        }
    }

    StringBuilder builder = {};
    DEFER(destroy(&builder));

    if (sheet->kind == SHEET_EXECUTABLE) {
        String exe_name = sheet->name;
        String exe_dir = t_format("%S/%S", App.build_files_directory, filename_without_path(sheet->full_path));

        platform_delete_file_or_directory(exe_dir);
        platform_create_directory(exe_dir);

        append_format(&builder, "cl /nologo /permissive- /W2");

        if (App.build_type == "debug") {
            append(&builder, " /Zi /D\"DEVELOPER\"");
        }

        FOR (sheet->symbols, symbol) {
            append_format(&builder, " /D\"%S\"", *symbol);
        }

        FOR (sheet->include_dirs, dir) {
            append_format(&builder, " /I\"%S\"", *dir);
        }

        append_format(&builder, " /Fe\"%S/%S\"", sheet->build_dir, exe_name); // NOTE: /Fe -> executable name
        append_format(&builder, " /Fo\"%S/\"", exe_dir); // NOTE: /Fo -> object file location, / means a folder for all files

        if (App.build_type == "debug") {
            append_format(&builder, " /Fd\"%S/\"", exe_dir);
        }

        FOR (sheet->sources, source) {
            append_format(&builder, " \"%S\"", *source);
        }

        append(&builder, " /link /SUBSYSTEM:CONSOLE /INCREMENTAL:NO");

        FOR (sheet->libraries, lib) {
            append_format(&builder, " \"%S\"", *lib);
        }

        print("Building Executable: %S ", sheet->name);
        flush_write_buffer(&Console.out);
    } else if (sheet->kind == SHEET_LIBRARY) {
        String object_dir = t_format("%S/%S", App.build_files_directory, filename_without_path(sheet->full_path));

        platform_delete_file_or_directory(object_dir);
        platform_create_directory(object_dir);

        // NOTE: storing the compiled object files
        DArray<String> object_files = {};
        DEFER(destroy(object_files));

        append(&builder, "cl /nologo /permissive- /W2 /c");

        if (App.build_type == "debug") {
            append(&builder, " /Zi /D\"DEVELOPER\"");
        }

        FOR (sheet->symbols, symbol) {
            append_format(&builder, " /D\"%S\"", *symbol);
        }

        FOR (sheet->include_dirs, dir) {
            append_format(&builder, " /I\"%S\"", *dir);
        }

        append_format(&builder, " /Fo\"%S/\"", object_dir); // NOTE: /Fo -> object file location, / means a folder for all files

        if (App.build_type == "debug") {
            append_format(&builder, " /Fd\"%S/\"", object_dir);
        }

        FOR (sheet->sources, source) {
            append_format(&builder, " \"%S\"", *source);

            append(object_files, format("%S/%S.%s", object_dir, filename_without_extension(*source), "obj"));
        }

        print("Building Library: %S ", sheet->name);
        flush_write_buffer(&Console.out);

        String command = temp_string(&builder);
        if (App.verbose) print("with command: %S\n", command);

        String output = platform_execute(command);
        b32 encountered_errors = process_diagnostics(output, &sheet->diagnostics);
        destroy_string(&output);

        if (encountered_errors) {
            FOR (object_files, file) {
                destroy_string(file);
            }

            print("(failure)\n");
            sheet->status = SHEET_STATUS_ERROR;

            return false;
        }

        reset(&builder);

        // TODO: the /ignore:4006 supresses the warning about warning LNK4006: __NULL_IMPORT_DESCRIPTOR already defined in other lib
        //       I don't know if this is of any importance for other libs so it is ignored for now
        append_format(&builder, "LIB /NOLOGO /ignore:4006 /OUT:\"%S\"", sheet->full_path);

        FOR (object_files, file) {
            append_format(&builder, " \"%S\"", *file);
            destroy_string(file);
        }

        FOR (sheet->libraries, lib) {
            append_format(&builder, " \"%S\"", lib);
        }
    } else {
        append(sheet->diagnostics, format(App.string_alloc, "Can only build Executables and Libraries. (sheet: %S)\n", sheet->name));
    }

    String command = temp_string(&builder);
    if (App.verbose) print("with command: %S\n", command);

    String output = platform_execute(command);
    DEFER(destroy_string(&output));

    b32 encountered_errors = process_diagnostics(output, &sheet->diagnostics);

    if (encountered_errors) {
        print("(failure)\n");
        sheet->status = SHEET_STATUS_ERROR;
    } else {
        sheet->status = SHEET_STATUS_READY;
        print("(done)\n");
    }

    return !encountered_errors;
}


INTERNAL Compiler KnownCompilers[] = {
    MSVC_Compiler
};


INTERNAL Compiler *find_compiler(String name) {
    for (s32 i = 0; i < ARRAY_SIZE(KnownCompilers); i += 1) {
        if (equal(KnownCompilers[i].name, name)) return &KnownCompilers[i];
    }

    return 0;
}



INTERNAL b32 build_executable(Blueprint *blueprint, Sheet *sheet) {
    if (sheet->group != App.group) return true;

    assert(sheet->kind == SHEET_EXECUTABLE);


    Compiler *compiler = find_compiler(sheet->compiler);
    if (compiler == 0) {
        print("Unknown compiler %S specified for Executable %S.\n", sheet->compiler, sheet->name);

        return false;
    }

    platform_create_directory(sheet->build_dir);

    b32 encountered_errors = false;

    if (!compiler->build(default_allocator(), blueprint, sheet)) {
        encountered_errors = true;
    }

    FOR (sheet->diagnostics, diag) {
        print("%S\n", diag);
    }

    return !encountered_errors;
}

INTERNAL String last_directory(String path) {
    if (path.size == 0) return path;
    if (path[path.size - 1] == '/') path.size -= 1;

    for (s32 i = path.size; i > 0; i -= 1) {
        if (path[i - 1] == '/') {
            return {&path[i], path.size - i};
        }
    }

    return path;
}

enum ApplicationMode {
    APP_MODE_REGULAR,
    APP_MODE_REGISTER,
    APP_MODE_ERROR,
};

INTERNAL ApplicationMode process_arguments(Array<String> args) {
    if (args.size > 1) {
        if (args[1] == "register") {
            if (args.size > 2) {
                print("NOTE: Additional arguments provided after register. Will be ignored.\n");
            }

            return APP_MODE_REGISTER;
        }
    }

    for (s64 i = 1; i < args.size; i += 1) {
        if (args[i] == "--build_type") {
            if (args.size < i) {
                print("NOTE: Argument 'build_type' is missing a type and will be ignored.\n");

                break;
            }

            i += 1;
            App.build_type = args[i];
        } else if (args[i] == "--group") {
            i += 1;
            if (args.size <= i) {
                print("NOTE: Argument 'group' is missing a name and will be ignored.\n");

                break;
            }

            App.group = args[i];
        } else if (args[i] == "--verbose") {
            App.verbose = true;
        } else {
            print("NOTE: Unknown argument %S. Will be ignored.\n", args[i]);
        }
    }

    return APP_MODE_REGULAR;
}

INTERNAL TargetPlatformInfo TargetPlatforms[TARGET_COUNT] = {
    /* TARGET_UNDEFINED */ {},
    /* TARGET_WIN32     */ {".exe", ".lib", ".dll"},
};


/* NOTE: is a cache really necessary? The reader should be just as fast as long as there is no check for changes to the files
INTERNAL b32 process_cache_file() {
    u64 cache_time     = platform_file_time(App.cache_file);
    u64 blueprint_time = platform_file_time(App.blueprint_file);

    // TODO: time is 0 if the file does not exist
    if (cache_time && blueprint_time < cache_time) {
        String cache = read_entire_file(App.cache_file);
        DEFER(destroy_string(&cache));

        Compiler *compiler = 0;

        Array<String> lines = split_by_line_ending(cache);
        FOR (lines, line) {
            if (line->size == 0) continue;

            if (starts_with(*line, "s ")) {
                assert(compiler);

                String tmp = shrink_front(*line, 2);
                print("%S\n", tmp);
            } else if (starts_with(*line, "c ")) {
                assert(compiler);

                String tmp = shrink_front(*line, 2);

                if (!compiler->run(default_allocator(), tmp)) {
                    print("Could not rebuild blueprint. Trying to build by reparsing.\n");
                    platform_delete_file_or_directory(App.cache_file);

                    return false;
                }
            } else if (starts_with(*line, "p ")) {
                String tmp = shrink_front(*line, 2);

                // TODO: better error checking
                compiler = find_compiler(tmp);
            } else {
                // TODO: not die but exit
                platform_delete_file_or_directory(App.cache_file);
                die("Malformed cache file. Cache will be deleted.\n");

                return true;
            }
        }

        print("\nRebuild finished\n");

        return true;
    }

    return false;
}
*/


s32 application_main(Array<String> args) {
    App.string_storage = allocate_arena(MEGABYTES(4));
    App.string_alloc   = {(AllocatorFunc*)allocate_from_arena, &App.string_storage};
    DEFER(destroy(&App.string_storage));

    String app_data_dir = platform_application_data_directory();
    if (app_data_dir.size == 0) {
        print("Could not load config file path.");

        return -1;
    }

    App.starting_directory    = allocate_string(App.string_alloc, platform_current_working_directory());
    App.build_files_directory = format(App.string_alloc, "%S/.bricks",    App.starting_directory);
    App.config_directory      = format(App.string_alloc, "%S/bricks",     app_data_dir);
    App.blueprint_file        = format(App.string_alloc, "%S/blueprint",  App.starting_directory);
    App.brickyard_file        = format(App.string_alloc, "%S/brick.yard", App.config_directory);

    platform_create_directory(App.build_files_directory);

#ifdef OS_WINDOWS
    App.target_platform = TARGET_WIN32;
#endif

    App.build_type = "debug";

    ApplicationMode mode = process_arguments(args);

    if (mode == APP_MODE_REGISTER) {
        String register_name = last_directory(App.starting_directory);

        if (!App.brickyard.loaded) {
            LoadResult load_result = load_brickyard(&App);
        }

        FOR (App.brickyard.entries, entry) {
            if (entry->name == register_name) {
                print("Blueprint %S already registered.\n", register_name);

                return -1;
            }
        }

        append(App.brickyard.entries, {register_name, App.starting_directory});

        platform_delete_file_or_directory(App.brickyard_file);
        PlatformFile file = platform_create_file_handle(App.brickyard_file, PLATFORM_FILE_WRITE);

        FOR (App.brickyard.entries, entry) {
            format(&file, "%S {%S}\n", entry->name, entry->path);
        }

        platform_close_file_handle(&file);
        
        print("Created Brick %S.\n", register_name);

        return 0;
    }

    assert(App.target_platform);
    App.target_info = TargetPlatforms[App.target_platform];

    b32 encountered_build_errors = false;

    Blueprint blueprint = parse_blueprint_file(App.blueprint_file);
    DEFER(free_blueprint(&blueprint));

    if (blueprint.valid == false) {
        print("Stopping build.\n");
        return -1;
    }

    FOR (blueprint.sheets, sheet) {
        reset_temporary_storage();

        if (sheet->kind == SHEET_EXECUTABLE) {
            if (!build_executable(&blueprint, sheet)) {
                encountered_build_errors = true;
            }
        }
    }

    if (encountered_build_errors) {
        print("\nBuild finished with errors.\n");
    } else {
        /*
        String cache = to_allocated_string(&App.cache_builder);
        DEFER(destroy_string(&cache));

        PlatformFile cache_file = platform_create_file_handle(App.cache_file, PLATFORM_FILE_WRITE);
        if (cache_file.open) {
            platform_write(&cache_file, cache.data, cache.size);
            platform_close_file_handle(&cache_file);
        } else {
            print("ERROR: Could not create cache file.\n");
        }
        */

        print("\nBuild finished\n");
    }

    return encountered_build_errors;
}


#ifdef OS_WINDOWS
#include "win32_platform.cpp"
#endif


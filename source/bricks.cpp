#include "bricks.h"

#include "platform.h"
#include "array.h"
#include "io.h"
#include "string_builder.h"
#include "arena.h"
#include "blueprint.h"

#include "core_compilers.h"


ApplicationState App;


b32 be_verbose() {
    return App.verbose;
}

void load_core_compilers() {
    append(&App.compilers, load_msvc());
}


INTERNAL String enum_string(EntityKind kind) {
    String const lookup[ENTITY_COUNT] = {
        "None",
        "Brick",
        "Executable",
        "Library",
    };

    assert(kind < ENTITY_COUNT);
    return lookup[kind];
}


INTERNAL Compiler *find_compiler(String name) {
    FOR (App.compilers, compiler) {
        if (compiler->name == name) return compiler;
    }

    return 0;
}

INTERNAL String combine_entity_path(String bp_folder, String build_folder, String name, String extension) {
    StringBuilder builder = {};
    if (bp_folder != "") {
        bp_folder = remove_trailing_slashes(bp_folder);

        append(&builder, bp_folder);
        append(&builder, '/');
    }

    if (build_folder != "") {
        build_folder = remove_leading_slashes(build_folder);
        build_folder = remove_trailing_slashes(build_folder);

        append(&builder, build_folder);
        append(&builder, '/');
    }

    name = remove_leading_slashes(name);
    append(&builder, name);

    if (extension != "") {
        append(&builder, '.');
        append(&builder, extension);
    }
    
    return to_allocated_string(&builder, App.string_alloc);
}

INTERNAL String combine_intermediate_path(String bp_folder, String name, String extension) {
    StringBuilder builder = {};
    if (bp_folder != "") {
        bp_folder = remove_trailing_slashes(bp_folder);

        append(&builder, bp_folder);
        append(&builder, '/');
    }

    append(&builder, ".bricks/");

    name = remove_leading_slashes(name);
    append(&builder, name);

    if (extension != "") {
        append(&builder, '.');
        append(&builder, extension);
    }
    
    return to_allocated_string(&builder, App.string_alloc);
}

INTERNAL void append_unique(List<String> *list, String str) {
    FOR (*list, elem) {
        if (*elem == str) return;
    }

    append(list, str);
}

// TODO: This is very slow and bad.
//       Alternatively the array could be sorted and then binary searched.
INTERNAL void merge_arrays(List<String> *dest, Array<String> src) {
    FOR (src, string) {
        append_unique(dest, *string);
    }
}

INTERNAL void add_brick_to_entity(Entity *entity, Entity *brick) {
    assert(brick->kind == ENTITY_BRICK);

    merge_arrays(&entity->include_folders, brick->include_folders);
    merge_arrays(&entity->sources,      brick->sources);
    merge_arrays(&entity->libraries,    brick->libraries);
    merge_arrays(&entity->symbols,      brick->symbols);
}

INTERNAL void build(Blueprint *blueprint, Entity *entity) {
    Compiler *compiler = find_compiler(entity->compiler);
    if (compiler == 0) {
        String msg = t_format("Unknown compiler %S specified for Entity %S.\n", entity->compiler, entity->name);
        add_diagnostic(DIAG_ERROR, msg);
        return;
    }

    // NOTE: Always print all diagnostics on failure or success.
    DEFER(print_diagnostics(entity));

    FOR (entity->dependencies, dep) {
        Blueprint *module = find_submodule(blueprint, dep->module);
        if (!module) {
            blueprint->status = BLUEPRINT_ERROR;
            return;
        }

        Entity *sub = find_dependency(module, dep->entity);
        if (!sub) {
            blueprint->status = BLUEPRINT_ERROR;
            return;
        }

        if (!sub) {
            String msg = {};
            if (dep->module == "") {
                msg = t_format("Could not find dependency %S.", dep->entity);
            } else {
                msg = t_format("Could not find dependency %S.%S.", dep->module, dep->entity);
            }

            add_diagnostic(entity, DIAG_ERROR, msg);
            return;
        }

        switch (sub->kind) {
        case ENTITY_BRICK: {
            add_brick_to_entity(entity, sub);
        } break;

        case ENTITY_LIBRARY: {
            if (sub->status != ENTITY_STATUS_READY) build(module, sub);

            if (sub->status == ENTITY_STATUS_READY) {
                append(&entity->libraries, sub->file_path);
                merge_arrays(&entity->libraries, sub->libraries);
            } else {
                String msg = t_format("Could not build library %S.", sub->name);
                add_diagnostic(entity, DIAG_ERROR, msg);
                return;
            }
        } break;

        default:
            // TODO: Executables should be dependencies as well, they will just not be added to the entity.
            String msg = t_format("Can only add Bricks and Libraries as dependencies at the moment. (entity: %S, dep: %S)\n", entity->name, sub->name);
            add_diagnostic(entity, DIAG_ERROR, msg);

            return;
        }
    }

    if (blueprint->name == "") {
        print("Building %S %S\n", enum_string(entity->kind), entity->name);
    } else {
        print("Building %S %S.%S\n", enum_string(entity->kind), blueprint->name, entity->name);
    }

    String extension = {};
    if (entity->kind == ENTITY_EXECUTABLE) {
        extension = App.target_info.exe;
    } else if (entity->kind == ENTITY_LIBRARY) {
        if (entity->lib_kind == STATIC_LIBRARY) {
            extension = App.target_info.static_lib;
        } else {
            entity->status = ENTITY_STATUS_ERROR;

            String msg = t_format("Libary %S could not be build (Not implemented).", entity->name);
            add_diagnostic(DIAG_ERROR, msg);
            return;
        }
    } else {
        entity->status = ENTITY_STATUS_ERROR;

        String msg = t_format("Entity %S could not be build (Not implemented).", entity->name);
        add_diagnostic(DIAG_ERROR, msg);
        return;
    }

    entity->intermediate_folder = combine_intermediate_path(blueprint->path, entity->name, extension);

    // NOTE: Static libs are in the intermediate folder to not pollute the build folder by default.
    if (entity->kind == ENTITY_LIBRARY && entity->lib_kind == STATIC_LIBRARY && entity->build_folder == "") {
        entity->file_path = combine_entity_path(entity->intermediate_folder, "", entity->name, extension);
    } else {
        entity->file_path = combine_entity_path(blueprint->path, entity->build_folder, entity->name, extension);
    }

    platform_create_all_folders(path_without_filename(entity->file_path));
    platform_create_folder(entity->intermediate_folder);

    compiler->generate_commands(DefaultAllocator, blueprint, entity);

    for (s32 i = 0; i < entity->build_command_count; i += 1) {
        String command = entity->build_commands[i];

        // TODO: print might be not a great option. Maybe do something like a DIAG_MESSAGE?
        if (be_verbose()) print("with command: %S\n", command);

        // TODO: The context could use a fixed size buffer to remove allocations.
        //       I don't think outputs over 1mb would be helpful in any way.
        auto context = platform_execute(command);
        if (context.error) {
            log_error("Could not run command %S.", command);
            return;
        }

        compiler->process_diagnostics(entity, context.output);

        destroy(&context.output);
    }

    if (entity->status == ENTITY_STATUS_ERROR) {
        App.has_errors = true;
    } else {
        entity->status = ENTITY_STATUS_READY;
    }
}

INTERNAL void process_imports(Blueprint *blueprint) {
    FOR (blueprint->imports, import) {
        if (import->local) {
            String file = t_format("%S/blueprint", import->name);
            if (!platform_file_exists(file)) {
                add_diagnostic(DIAG_ERROR, t_format("No blueprint file in folder %S found.", import->name));
                return;
            }

            parse_blueprint_file(&import->blueprint, file);
        } else {
            String yard_folder = find(&App.brickyard, import->name);

            String file = t_format("%S/blueprint", yard_folder);
            if (!platform_file_exists(file)) {
                add_diagnostic(DIAG_ERROR, t_format("Missing blueprint file in Brickyard entry for %S. Was it deleted maybe?", import->name));
                return;
            }

            parse_blueprint_file(&import->blueprint, file);
        }
    }
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
    APP_MODE_ERROR, // TODO: Is an error code needed here?
    APP_MODE_BUILDING,
    APP_MODE_REGISTER,
};
struct StartupOptions {
    ApplicationMode mode;

    String build_type;
    String group;
    String platform;

    String register_name;

    b32 verbose;
};

INTERNAL StartupOptions process_arguments(Array<String> args) {
    StartupOptions result = {};

    if (args.size > 1) {
        if (args[1] == "register") {
            if (args.size > 2) {
                result.register_name = args[2];
            }

            result.mode = APP_MODE_REGISTER;
            return result;
        }
    }

    for (s64 i = 1; i < args.size; i += 1) {
        if (args[i] == "--build_type") {
            if (args.size < i) {
                print("NOTE: Argument 'build_type' is missing a type and will be ignored.\n");

                break;
            }

            i += 1;
            result.build_type = args[i];
        } else if (args[i] == "--group") {
            i += 1;
            if (args.size <= i) {
                print("NOTE: Argument 'group' is missing a name and will be ignored.\n");

                break;
            }

            result.group = args[i];
        } else if (args[i] == "--platform") {
            i += 1;
            if (args.size <= i) {
                print("NOTE: Argument 'platform' is missing an argument and will be ignored.\n");

                break;
            }

            result.platform = args[i];
        } else if (args[i] == "--verbose") {
            result.verbose = true;
        } else {
            print("NOTE: Unknown argument %S. Will be ignored.\n", args[i]);
        }
    }

    return result;
}

void add_diagnostic(DiagnosticKind kind, String msg) {
    Diagnostic diag = {};
    diag.kind = kind;
    diag.message = allocate_string(msg, App.string_alloc);

    if (kind == DIAG_ERROR) App.has_errors = true;
    
    append(&App.diagnostics, diag);
}

INTERNAL void print_diagnostics() {
    FOR (App.diagnostics, diag) {
        print("%S\n", diag->message);
    }
}


INTERNAL b32 get_platform_info(String platform, TargetPlatformInfo *info) {
    b32 result = false;

    if (platform == "win32") {
        *info = {"exe", "lib", "dll"};
        result = true;
    }

    return result;
}


s32 application_main(Array<String> args) {
    init(&App.string_storage, MEGABYTES(4));
    DEFER(destroy(&App.string_storage));

    App.string_alloc = make_arena_allocator(&App.string_storage);

    String config_folder = platform_home_folder();
    if (config_folder == "") {
        add_diagnostic(DIAG_ERROR, "Could not retrieve configuration path.");
        print_diagnostics();

        return -1;
    }

    String current_folder = platform_current_folder(App.string_alloc);
    if (current_folder == "") {
        add_diagnostic(DIAG_ERROR, "Could not retrieve current path.");
        print_diagnostics();

        return -1;
    }

    App.starting_folder    = current_folder;
    App.build_files_folder = format(App.string_alloc, "%S/.bricks",    App.starting_folder);
    App.config_folder      = format(App.string_alloc, "%S/bricks",     config_folder);
    App.brickyard_file     = format(App.string_alloc, "%S/brick.yard", App.config_folder);

#ifdef OS_WINDOWS
    App.target_platform = "win32";
#endif

    // TODO: Should debug even be the default?
    App.build_type = "debug";

    load_brickyard(&App.brickyard, App.brickyard_file, App.string_alloc);
    DEFER(save_brickyard(&App.brickyard, App.brickyard_file));

    load_core_compilers();

    StartupOptions options = process_arguments(args);
    if (options.mode == APP_MODE_REGISTER) {
        String name = options.register_name;
        if (name == "") name = last_directory(App.starting_folder);


        FOR (App.brickyard.entries, entry) {
            if (entry->name == name) {
                print("Blueprint %S already registered.\n", name);

                return -1;
            }
        }

        add(&App.brickyard, name, "", App.starting_folder);

        // NOTE: Brickyard will be saved on scope exit anyways.
        print("Created Brickyard entry for %S.\n", name);

        return 0;
    }

    if (options.verbose) App.verbose = true;
    
    App.group = options.group;

    platform_create_folder(App.build_files_folder);

    if (!get_platform_info(App.target_platform, &App.target_info)) {
        add_diagnostic(DIAG_ERROR, t_format("Unsupported platform %S.", App.target_platform));
        print_diagnostics();

        return -1;
    }

    Blueprint main_blueprint = {};
    DEFER(destroy(&main_blueprint));

    parse_blueprint_file(&main_blueprint, "blueprint");

    process_imports(&main_blueprint);

    b32 has_stuff_to_build = false;
    if (!App.has_errors) {
        FOR (main_blueprint.entitys, entity) {
            if (entity->kind == ENTITY_EXECUTABLE) {
                if ((App.group == "" && entity->groups.size == 0) ||
                    (contains((Array<String>)entity->groups, App.group))) {
                    build(&main_blueprint, entity);
                    has_stuff_to_build = true;
                }
            }
        }
    }

    s32 result = 0;
    if (App.has_errors) {
        print_diagnostics();

        print("\nBuild aborted.\n");
        result = -1;
    } else {
        print_diagnostics();
        if (!has_stuff_to_build) print("Nothing to build.\n");

        print("\nBuild finished.\n");
    }

    return result;
}


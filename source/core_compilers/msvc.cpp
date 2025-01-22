#include "blueprint.h"
#include "bricks.h"
#include "string_builder.h"
#include "platform.h"



// TODO: Move into a path.h or similar.
INTERNAL String filename_without_path(String path) {
    String result = path;

    for (s64 i = path.size; i > 0; i -= 1) {
        s64 index = i - 1;
        if (path[index] == '/' || path[index] == '\\') {
            result = {path.data + i, path.size - i};
            break;
        }
    }

    return result;
}

INTERNAL String filename_without_extension(String filename) {
    String result = filename_without_path(filename);

    s64 pos = find_last(result, '.');
    if (pos != -1) result.size = pos;

    return result;
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

INTERNAL void process_diagnostics(Entity *entity, String output) {
    while (output.size) {
        String line = remove_line(&output);

        if (contains(line, ": error ")) {
            add_diagnostic(entity, DIAG_ERROR, line);
            entity->status = ENTITY_STATUS_ERROR;
        } else if (contains(line, ": fatal error ")) {
            add_diagnostic(entity, DIAG_ERROR, line);
            entity->status = ENTITY_STATUS_ERROR;
        } else if (contains(line, " Command line error ")) {
            add_diagnostic(entity, DIAG_ERROR, line);
            entity->status = ENTITY_STATUS_ERROR;
        } else if (contains(line, ": warning")) {
            add_diagnostic(entity, DIAG_WARNING, line);
        } else if (contains(line, ": note: ")) {
            add_diagnostic(entity, DIAG_NOTE, line);
        }
    }
}

INTERNAL void msvc_build_command(Allocator alloc, Blueprint *blueprint, Entity *entity) {
    SCOPE_TEMP_STORAGE();

    if (entity->status == ENTITY_STATUS_READY) return;
    if (entity->status == ENTITY_STATUS_ERROR) return;

    StringBuilder builder = {};
    DEFER(destroy(&builder));

    if (entity->kind == ENTITY_EXECUTABLE) {
        if (entity->sources.size == 0) {
            add_diagnostic(entity, DIAG_ERROR, t_format("Executable %S has no source file(s) to build.", entity->name));
            // TODO: Maybe don't error and just report it?
            //       So the build can finish if this entity is unused.
            entity->status = ENTITY_STATUS_ERROR;
            return;
        }

        append(&builder, "cl /nologo /permissive- /W2");

        if (blueprint->build_type == "debug") {
            append(&builder, " /Zi");
        }

        FOR (entity->symbols, symbol) {
            format(&builder, " /D\"%S\"", *symbol);
        }

        FOR (entity->include_folders, dir) {
            format(&builder, " /I\"%S\"", *dir);
        }

        format(&builder, " /Fe\"%S\"", entity->file_path); // NOTE: /Fe -> executable name
        format(&builder, " /Fo\"%S/\"", entity->intermediate_folder); // NOTE: /Fo -> object file location, / means a folder for all files

        if (blueprint->build_type == "debug") {
            String folder = path_without_filename(entity->file_path);
            if (folder != "") format(&builder, " /Fd\"%S/\"", folder);
        }

        FOR (entity->sources, source) {
            format(&builder, " \"%S\"", *source);
        }

        append(&builder, " /link /SUBSYSTEM:CONSOLE /INCREMENTAL:NO");

        FOR (entity->libraries, lib) {
            format(&builder, " \"%S\"", *lib);
        }

        String command = temp_string(&builder);
        if (be_verbose()) print("with command: %S\n", command);

        auto context = platform_execute(command);
        if (context.error) {
            log_error("Could not run command %S.", command);
            return;
        }
        DEFER(destroy(&context.output));

        process_diagnostics(entity, context.output);
    } else if (entity->kind == ENTITY_LIBRARY) {
        // NOTE: storing the compiled object files
        List<String> object_files = {};
        object_files.allocator = alloc;
        DEFER(destroy(&object_files));

        append(&builder, "cl /nologo /permissive- /W2 /c");

        if (blueprint->build_type == "debug") {
            append(&builder, " /Zi");
        }

        FOR (entity->symbols, symbol) {
            format(&builder, " /D\"%S\"", *symbol);
        }

        FOR (entity->include_folders, dir) {
            format(&builder, " /I\"%S\"", *dir);
        }

        // TODO: Shared lib needs to be in the build folder.
        format(&builder, " /Fo\"%S/\"", entity->intermediate_folder); // NOTE: /Fo -> object file location, / means a folder for all files

        if (blueprint->build_type == "debug") {
            format(&builder, " /Fd\"%S/\"", entity->intermediate_folder);
        }

        FOR (entity->sources, source) {
            format(&builder, " \"%S\"", *source);

            String obj = format(alloc, "%S/%S.obj", entity->intermediate_folder, filename_without_extension(*source));
            append(&object_files, obj);
        }

        String command = to_allocated_string(&builder);
        if (be_verbose()) print("with command: %S\n", command);

        auto context = platform_execute(command);
        if (context.error) {
            log_error("Could not run command %S.", command);
            return;
        }

        process_diagnostics(entity, context.output);
        destroy(&context.output);
        destroy(&command);

        if (entity->status == ENTITY_STATUS_ERROR) return;

        reset(&builder);

        format(&builder, "LIB /NOLOGO /OUT:\"%S\"", entity->file_path);

        FOR (object_files, file) {
            format(&builder, " \"%S\"", *file);
            destroy(file);
        }

        command = to_allocated_string(&builder);
        if (be_verbose()) print("with command: %S\n", command);

        context = platform_execute(command);
        if (context.error) {
            log_error("Could not run command %S.", command);
            return;
        }

        process_diagnostics(entity, context.output);
        destroy(&context.output);
        destroy(&command);
    } else {
        add_diagnostic(entity, DIAG_ERROR, t_format("Can only build Executables and Libraries. (entity: %S)\n", entity->name));
        return;
    }

    return;
}


Compiler load_msvc() {
    Compiler result = {};
    result.name  = "msvc";
    result.build = msvc_build_command;

    return result;
}


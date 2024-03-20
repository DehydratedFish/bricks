#pragma once

#include "bricks.h"

#ifdef DEVELOPER
void fire_assert(char const *msg, char const *func, char const *file, int line);
#define assert(expr) (void)((expr) || (fire_assert(#expr, __func__, __FILE__, __LINE__),0))
#else
#define assert(expr)
#endif // DEVELOPER


typedef void RenderFunc(struct GameState*);


enum {
    PLATFORM_FILE_READ,
    PLATFORM_FILE_WRITE,
    PLATFORM_FILE_APPEND,
};
struct PlatformFile {
    void *handle;
    MemoryBuffer read_buffer;
    MemoryBuffer write_buffer;

    b32 open;
};


PlatformFile platform_create_file_handle(String filename, u32 mode);
void   platform_close_file_handle(PlatformFile *file);
s64    platform_file_size(PlatformFile *file);
String platform_read(PlatformFile *file, u64 offset, void *buffer, s64 size);
s64    platform_write(PlatformFile *file, void const *buffer, s64 size);
s64    platform_write(PlatformFile *file, u64 offset, void const *buffer, s64 size);

u64 platform_file_time(String file);

void platform_create_directory(String path);
void platform_delete_file_or_directory(String path);

b32 platform_create_symlink(String link, String path);
String platform_get_full_path(String file);

enum {
    READ_ENTIRE_FILE_OK,
    READ_ENTIRE_FILE_NOT_FOUND,
    READ_ENTIRE_FILE_READ_ERROR,
};
String read_entire_file(String file, u32 *status = 0);



struct PlatformConsole {
    PlatformFile out;
};

extern PlatformConsole Console;


Array<String> platform_directory_listing(String path);
void platform_destroy_directory_listing(Array<String> *listing);

void *platform_allocate_raw_memory(s64 size);
void  platform_free_raw_memory(void *ptr);

String platform_get_executable_path();
String platform_current_working_directory();
String platform_application_data_directory();
b32 platform_file_exists(String file);

String platform_execute(String command);


s32 application_main(Array<String> args);


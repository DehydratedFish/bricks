#include "platform.h"

#define UNICODE
#define _UNICODE


#undef OS_WINDOWS
#define WIN32_LEAN_AND_MEAN
#include "windows.h"
#include "windowsx.h"
#include "shlobj_core.h"
#include "shlwapi.h"
#include "shellapi.h"
#include "objbase.h"
#include "dbghelp.h"


INTERNAL char NarrowBuffer[2048];
INTERNAL MemoryBuffer allocate_memory_buffer(Allocator alloc, s64 size) {
    MemoryBuffer buffer = {};
    buffer.memory = ALLOC(alloc, u8, size);
    buffer.used   = 0;
    buffer.alloc  = size;

    return buffer;
}

INTERNAL void free_memory_buffer(Allocator alloc, MemoryBuffer *buffer) {
    deallocate(alloc, buffer->memory, buffer->alloc);

    INIT_STRUCT(buffer);
}


#ifdef USE_RPMALLOC

#include "rpmalloc.h"
INTERNAL void *rp_malloc_alloc_func(void *, s64 size, void *old, s64 old_size) {
    void *result = 0;

#ifdef DEVELOPER
    auto arena = get_toolbox()->temporary_storage;
    if (old > arena.memory && old < arena.memory + arena.alloc) {
        print("Freeing memory from temporary storage with another allocator.\n");
        die("Ohh nose!!!! Be careful with temporary data. Lost 2 debugging already.\n");
    }
#endif

    if (old) {
        if (size) {
            result = rprealloc(old, size);
        } else {
            rpfree(old);
        }
    } else {
        result = rpcalloc(1, size);
    }

    return result;
}

Allocator RpmallocAllocator = {rp_malloc_alloc_func, 0};

#endif // USE_RPMALLOC

INTERNAL void *cstd_alloc_func(void *, s64 size, void *old, s64 old_size) {
    void *result = 0;

    if (old) {
        if (size) {
            result = realloc(old, size);
        } else {
            free(old);
        }
    } else {
        result = calloc(1, size);
    }

    return result;
}

Allocator CstdAllocator = {cstd_alloc_func, 0};


INTERNAL Allocator   DefaultAllocator;
INTERNAL MemoryArena TemporaryStorage;

// TODO: Remove the toolbox stuff and return back to per function allocators.
Allocator default_allocator() {
    return DefaultAllocator;
}

Allocator change_default_allocator(Allocator alloc) {
    Allocator old = DefaultAllocator;
    DefaultAllocator = alloc;

    return old;
}

Allocator temporary_allocator() {
    return make_arena_allocator(&TemporaryStorage);
}

s64 temporary_storage_mark() {
    return TemporaryStorage.used;
}

void temporary_storage_rewind(s64 mark) {
    TemporaryStorage.used = mark;
}

void reset_temporary_storage() {
    TemporaryStorage.used = 0;
}

PlatformConsole Console;

#ifndef PLATFORM_CONSOLE_BUFFER_SIZE
#define PLATFORM_CONSOLE_BUFFER_SIZE 4096
#endif

int main_main() {
    CoInitialize(0);
    DEFER(CoUninitialize());

#ifdef USE_RPMALLOC
    rpmalloc_initialize();
    DEFER(rpmalloc_finalize());
    DefaultAllocator = RpmallocAllocator;
#else
    DefaultAllocator = CstdAllocator;
#endif // USE_RPMALLOC

    TemporaryStorage = allocate_arena(KILOBYTES(32));
    DEFER(destroy(&TemporaryStorage));

    Console.out.handle = GetStdHandle(STD_OUTPUT_HANDLE);
    Console.out.write_buffer = allocate_memory_buffer(default_allocator(), PLATFORM_CONSOLE_BUFFER_SIZE);
    Console.out.open   = true;
    DEFER(free_memory_buffer(default_allocator(), &Console.out.write_buffer));

    wchar_t *cmd_line = GetCommandLineW();

    int cmd_arg_count;
    wchar_t **cmd_args = CommandLineToArgvW(cmd_line, &cmd_arg_count);
    DEFER(LocalFree(cmd_args));

    Array<String> args = ALLOCATE_ARRAY(String, cmd_arg_count);
    DEFER(destroy_array(&args));

    for (int i = 0; i < cmd_arg_count; i += 1) {
        String16 arg = {(u16*)cmd_args[i], (s64)wcslen(cmd_args[i])};
        args[i] = to_utf8(default_allocator(), arg);
    }

    s32 status = application_main(args);

    FOR (args, arg) {
        destroy_string(arg);
    }

    flush_write_buffer(&Console.out);

    return status;
}

int APIENTRY WinMain(HINSTANCE, HINSTANCE, PSTR, int) {
    return main_main();
}

int main() {
    return main_main();
}

INTERNAL wchar_t WidenBuffer[2048];
INTERNAL wchar_t *widen(String str) {
    int chars = MultiByteToWideChar(CP_UTF8, 0, (LPCCH)str.data, str.size, WidenBuffer, 2048);
    WidenBuffer[chars] = L'\0';

    return WidenBuffer;
}

INTERNAL void show_message_box_and_crash(String message) {
    MessageBoxW(0, widen(message), L"Fatal Error", MB_OK | MB_ICONERROR);

    DebugBreak();
    ExitProcess(-1);
}

INTERNAL void convert_backslash_to_slash(wchar_t *buffer, s64 size) {
    for (s64 i = 0; i < size; i += 1) {
        if (buffer[i] == L'\\') buffer[i] = L'/';
    }
}
INTERNAL void convert_slash_to_backslash(wchar_t *buffer, s64 size) {
    for (s64 i = 0; i < size; i += 1) {
        if (buffer[i] == L'/') buffer[i] = L'\\';
    }
}


PlatformFile platform_create_file_handle(String filename, u32 mode) {
    DWORD win32_mode = 0;
    DWORD win32_open_mode = 0;

    switch (mode) {
    case PLATFORM_FILE_READ: {
        win32_mode = GENERIC_READ;
        win32_open_mode = OPEN_ALWAYS;
    } break;

    case PLATFORM_FILE_WRITE: {
        win32_mode = GENERIC_WRITE;
        win32_open_mode = CREATE_ALWAYS;
    } break;

    case PLATFORM_FILE_APPEND: {
        win32_mode = GENERIC_READ | GENERIC_WRITE;
        win32_open_mode = OPEN_ALWAYS;
    } break;

    default:
        die("Unknown file mode.");
    }

    PlatformFile result = {};

    String16 wide_filename = to_utf16(temporary_allocator(), filename, true);
    DEFER(deallocate(temporary_allocator(), wide_filename.data, wide_filename.size * sizeof(u16)));

    convert_slash_to_backslash((wchar_t*)wide_filename.data, wide_filename.size);

    HANDLE handle = CreateFileW((wchar_t*)wide_filename.data, win32_mode, 0, 0, win32_open_mode, FILE_ATTRIBUTE_NORMAL, 0);
    if (handle == INVALID_HANDLE_VALUE) {
        return result;
    }

    if (mode == PLATFORM_FILE_APPEND) {
        SetFilePointer(handle, 0, 0, FILE_END);
    }

    result.handle = handle;
    result.open   = true;

    return result;
}

void platform_close_file_handle(PlatformFile *file) {
    if (file->handle != INVALID_HANDLE_VALUE && file->handle != 0) {
        CloseHandle(file->handle);
    }

    INIT_STRUCT(file);
}

s64 platform_file_size(PlatformFile *file) {
    LARGE_INTEGER size;
    GetFileSizeEx(file->handle, &size);

    return size.QuadPart;
}

String platform_read(PlatformFile *file, u64 offset, void *buffer, s64 size) {
    // TODO: split reads if they are bigger than 32bit
    assert(size <= INT_MAX);

    if (!file->open) return {};

    OVERLAPPED ov = {0};
    ov.Offset     = offset & 0xFFFFFFFF;
    ov.OffsetHigh = (offset >> 32) & 0xFFFFFFFF;

    ReadFile(file->handle, buffer, size, 0, &ov);

    DWORD bytes_read = 0;
    GetOverlappedResult(file->handle, &ov, &bytes_read, TRUE);

    return {(u8*)buffer, bytes_read};
}

s64 platform_write(PlatformFile *file, u64 offset, void const *buffer, s64 size) {
    // TODO: split writes if they are bigger than 32bit
    assert(size <= INT_MAX);

    if (!file->open) return 0;

    OVERLAPPED ov = {0};
    ov.Offset     = offset & 0xFFFFFFFF;
    ov.OffsetHigh = (offset >> 32) & 0xFFFFFFFF;

    WriteFile(file->handle, buffer, size, 0, &ov);

    DWORD bytes_written = 0;
    BOOL status = GetOverlappedResult(file->handle, &ov, &bytes_written, TRUE);

    return bytes_written;
}

s64 platform_write(PlatformFile *file, void const *buffer, s64 size) {
    return platform_write(file, ULLONG_MAX, buffer, size);
}

u64 platform_file_time(String file) {
    RESET_TEMP_STORAGE_ON_EXIT();

    String16 wide_file = to_utf16(temporary_allocator(), file, true);
    HANDLE handle = CreateFileW((wchar_t*)wide_file.data, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (handle == INVALID_HANDLE_VALUE) return 0;

    FILETIME time;
    GetFileTime(handle, 0, 0, &time);

    LARGE_INTEGER tmp;
    tmp.LowPart  = time.dwLowDateTime;
    tmp.HighPart = time.dwHighDateTime;

    CloseHandle(handle);

    return tmp.QuadPart;
}

void platform_create_directory(String path) {
    RESET_TEMP_STORAGE_ON_EXIT();

    String16 wide_path = to_utf16(temporary_allocator(), path, true);
    convert_slash_to_backslash((wchar_t*)wide_path.data, wide_path.size);

    CreateDirectoryW((wchar_t*)wide_path.data, 0);
}

void platform_create_directories_recursive(String path) {
    for (s64 pos = 0; pos < path.size; pos += 1) {
        if (path[pos] == '/' || path[pos] == '\\') {
            String sub = sub_string(path, 0, pos);
            platform_create_directory(sub);
        }
    }
    platform_create_directory(path);
}

void platform_delete_file_or_directory(String path) {
    RESET_TEMP_STORAGE_ON_EXIT();

    // NOTE: There need to be two \0 at the end of the string or SHFileOperation will continue reading the string
    // past the first zero. That happened before and I went insane because I could not understand why the program
    // suddenly deleted the blueprint file as well....
    //
    // NOTE: Can't use format here, the \0 would be ignored because of the length function
    String zeroed_path = allocate_string(temporary_allocator(), path.size + 1);
    copy_memory(zeroed_path.data, path.data, path.size);
    zeroed_path[zeroed_path.size - 1] = '\0';

    String16 wide_path = to_utf16(temporary_allocator(), zeroed_path, true);
    convert_slash_to_backslash((wchar_t*)wide_path.data, wide_path.size);

    SHFILEOPSTRUCTW op = {
        0, FO_DELETE, (wchar_t*)wide_path.data, 0,
        FOF_NO_UI, 0, 0, 0
    };

    SHFileOperationW(&op);
}

b32 platform_create_symlink(String link, String path) {
    if (link.size == 0 || path.size == 0) return false;

    String link_path = path_without_filename(link);
    platform_create_directories_recursive(link_path);

    String16 wide_link = to_utf16(temporary_allocator(), link, true);
    convert_slash_to_backslash((wchar_t*)wide_link.data, wide_link.size);

    String16 wide_path = to_utf16(temporary_allocator(), path, true);
    convert_slash_to_backslash((wchar_t*)wide_path.data, wide_path.size);

    b32 result = true;
    // TODO: check for differences in files and directories
    if (!CreateSymbolicLinkW((wchar_t*)wide_link.data, (wchar_t*)wide_path.data, SYMBOLIC_LINK_FLAG_DIRECTORY)) {
        result = false;
    }

    return result;
}

String platform_get_full_path(String file) {
    String16 wide_file = to_utf16(temporary_allocator(), file);

    s64 size = GetFullPathNameW((wchar_t*)wide_file.data, 0, 0, 0);
    wchar_t *buffer = ALLOC(temporary_allocator(), wchar_t, size);

    if (GetFullPathNameW((wchar_t*)wide_file.data, size, buffer, 0) != size - 1) {
        print("Could not retrieve full path for file %S.\n", file);
        return file;
    }

    String16 full = {(u16*)buffer, size};

    return to_utf8(temporary_allocator(), full);
}

String read_entire_file(String file, u32 *status) {
    String16 wide_file = to_utf16(temporary_allocator(), file, true);

    HANDLE handle = CreateFileW((wchar_t*)wide_file.data, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
    if (handle == INVALID_HANDLE_VALUE) {
        if (status) *status = READ_ENTIRE_FILE_NOT_FOUND;

        return {};
    }
    DEFER(CloseHandle(handle));

    LARGE_INTEGER size;
    GetFileSizeEx(handle, &size);

    String result = {};

    if (size.QuadPart) {
        result = allocate_string(size.QuadPart);

        s64 total = 0;
        while (total != result.size) {
            DWORD bytes_read = 0;

            if (!ReadFile(handle, result.data + total, size.QuadPart, &bytes_read, 0)) {
                if (status) *status = READ_ENTIRE_FILE_READ_ERROR;

                destroy_string(&result);
                return {};
            }

            total += bytes_read;
            size.QuadPart -= bytes_read;
        }
    }
    if (status) *status = READ_ENTIRE_FILE_OK;

    return result;
}


Array<String> platform_directory_listing(String path) {
    DArray<String> listing = {};

    String search = path;
    if (path.size == 0) {
        search = "*";
    } else if (!ends_with(path, "/") || !ends_with(path, "\\")) {
        search = t_format("%S/%s", path, "*");
    } else if (!ends_with(path, "/*") || !ends_with(path, "\\*")) {
        search = t_format("%S/%s", path, "\\*");
    }

    String16 wide_folder = to_utf16(temporary_allocator(), search);
    convert_slash_to_backslash((wchar_t*)wide_folder.data, wide_folder.size);

    WIN32_FIND_DATAW data;
    HANDLE handle = FindFirstFileW((wchar_t*)wide_folder.data, &data);
    if (handle == INVALID_HANDLE_VALUE) return listing;


    Allocator alloc = default_allocator();
    append(listing, to_utf8(alloc, {(u16*)data.cFileName, (s64)wcslen(data.cFileName)}));

    while (FindNextFileW(handle, &data)) {
        append(listing, to_utf8(alloc, {(u16*)data.cFileName, (s64)wcslen(data.cFileName)}));
    }

    FindClose(handle);

    return listing;
}

void platform_destroy_directory_listing(Array<String> *listing) {
    for (s64 i = 0; i < listing->size; i += 1) {
        destroy_string(&listing->memory[i]);
    }
    destroy_array(listing);
}

void *platform_allocate_raw_memory(s64 size) {
    void *result = VirtualAlloc(0, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (result == 0) show_message_box_and_crash("Could not allocate memory.");

    return result;
}

void platform_free_raw_memory(void *memory) {
    VirtualFree(memory, 0, MEM_RELEASE);
}


u32 const STACK_TRACE_SIZE = 64;
u32 const SYMBOL_NAME_LENGTH = 1024;

INTERNAL void print_stack_trace() {
    u8 InfoStorage[sizeof(SYMBOL_INFO) + SYMBOL_NAME_LENGTH];

    void *stack[STACK_TRACE_SIZE];
    HANDLE process = GetCurrentProcess();

    if (!SymInitialize(process, 0, TRUE)) {
        print("Could not retrieve stack trace.");
        return;
    }

    USHORT frames = CaptureStackBackTrace(0, STACK_TRACE_SIZE, stack, 0);
    SYMBOL_INFO *info = (SYMBOL_INFO *)InfoStorage;
    info->SizeOfStruct = sizeof(SYMBOL_INFO);
    info->MaxNameLen = SYMBOL_NAME_LENGTH - 1;

    for (int i = 2; i < frames - 2; i += 1) {
        if (SymFromAddr(process, (DWORD64)stack[i], 0, info))
            print("%p: %s\n", info->Address, info->Name);
        else
            print("0x000000000000: ???");
    }

    SymCleanup(process);
}

void die(String msg) {
    print("Fatal Error: %S\n\n", msg);
    print_stack_trace();

    DebugBreak();
    ExitProcess(-1);
}

void fire_assert(char const *msg, char const *func, char const *file, int line) {
    print("Assertion failed: %s\n", msg);
    print("\t%s\n\t%s:%d\n\n", file, func, line);

    print_stack_trace();

    DebugBreak();
    ExitProcess(-1);
}


String platform_get_executable_path() {
    wchar_t PathBuffer[MAX_PATH];
    s64 size = GetModuleFileNameW(0, PathBuffer, MAX_PATH);
    assert(size);

    convert_backslash_to_slash(PathBuffer, size);

    return to_utf8(temporary_allocator(), {(u16*)PathBuffer, size});
}

String platform_current_working_directory() {
    s32 length = GetCurrentDirectoryW(0, 0);
    wchar_t *buffer = ALLOC(temporary_allocator(), wchar_t, length);

    length = GetCurrentDirectoryW(length, buffer);
    convert_backslash_to_slash(buffer, length);

    return to_utf8(temporary_allocator(), {(u16*)buffer, (s64)length});
}

String platform_application_data_directory() {
    wchar_t *buffer;
    if (SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, 0, &buffer) != S_OK) return {};

    String16 path = {(u16*)buffer, (s64)wcslen(buffer)};
    convert_backslash_to_slash(buffer, path.size);
    
    String result = to_utf8(temporary_allocator(), path);
    CoTaskMemFree(buffer);

    return result;
}

b32 platform_file_exists(String file) {
    String16 wide_file = to_utf16(temporary_allocator(), file, true);
    convert_slash_to_backslash((wchar_t*)wide_file.data, wide_file.size);

    return PathFileExistsW((wchar_t*)wide_file.data);
}

String platform_execute(String command) {
    String16 wide_command = to_utf16(temporary_allocator(), command, true);

    HANDLE read_pipe  = 0;
    HANDLE write_pipe = 0;

    SECURITY_ATTRIBUTES attributes = {};
    attributes.nLength              = sizeof(attributes);
    attributes.bInheritHandle       = TRUE;
    attributes.lpSecurityDescriptor = 0;

    assert(CreatePipe(&read_pipe, &write_pipe, &attributes, 0));
    assert(SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0));

    PROCESS_INFORMATION process = {};
    STARTUPINFO info = {};
    info.cb         = sizeof(info);
    info.hStdError  = write_pipe;
    info.hStdOutput = write_pipe;
    info.dwFlags   |= STARTF_USESTDHANDLES;

    info.cb = sizeof(info);
    BOOL status = CreateProcessW(0, (wchar_t*)wide_command.data, 0, 0, TRUE, 0, 0, 0, &info, &process);
    if (!status) {
        print("ERROR: Running command %S with error code %d. ", command, GetLastError());
        die("Could not start process.");
    }

    // NOTE: closing the write pipe because otherwise the handle will linger after the process finished
    //       and the last ReadFile will never return as the write handle stays open
    CloseHandle(write_pipe);

    StringBuilder builder = {};
    DEFER(destroy(&builder));

    s32 const buffer_size = 4096;
    u8 buffer[buffer_size];
    DWORD bytes_read = 0;

    BOOL read_ok = TRUE;
    while (read_ok) {
        read_ok = ReadFile(read_pipe, buffer, buffer_size, &bytes_read, 0);
        append(&builder, {buffer, bytes_read});
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    CloseHandle(process.hProcess);
    CloseHandle(process.hThread);
    
    CloseHandle(read_pipe);

    return to_allocated_string(&builder);
}


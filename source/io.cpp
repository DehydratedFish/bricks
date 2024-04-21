#include "bricks.h"
#include "platform.h"

#include <cstdarg>


s64 convert_string_to_s64(u8 *buffer, s32 buffer_size) {
    // TODO: Overflow detection. Limit string length for now.
    assert(buffer_size < 6);

    s64 result = 0;
    b32 is_negative = false;

    if (buffer_size == 0) {
        return result;
    }

    if (buffer[0] == '-') {
        is_negative = true;
        buffer += 1;
        buffer_size -= 1;
    }

    for (s32 i = 0; i < buffer_size; i += 1) {
        result *= 10;
        result += buffer[i] - '0';
    }

    if (is_negative) {
        result *= -1;
    }

    return result;
}

INTERNAL u8 CharacterLookup[] = "0123456789abcdefghijklmnopqrstuvwxyz";
INTERNAL u8 CharacterLookupUppercase[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

INTERNAL String convert_signed_to_string(u8 *buffer, s32 buffer_size, s64 signed_number, s32 base, b32 uppercase, b32 keep_sign) {
    assert(buffer_size >= 20);

    b32 is_negative;
    u64 number;

    if (signed_number < 0) {
        is_negative = true;
        number = -signed_number;
    } else {
        is_negative = false;
        number = signed_number;
    }

    u8 *lookup = uppercase ? CharacterLookupUppercase : CharacterLookup;

    u8 *ptr = buffer + buffer_size;
    s32 written = 0;
    do {
        s64 quot = number / base;
        s64 rem  = number % base;

        ptr -= 1;
        *ptr = lookup[rem];
        written += 1;
        number = quot;
    } while (number);

    if (is_negative || keep_sign) {
        ptr -= 1;
        *ptr = is_negative ? '-' : '+';
        written += 1;
    }

    String result = {ptr, written};
    return result;
}

INTERNAL String convert_unsigned_to_string(u8 *buffer, s32 buffer_size, u64 number, s32 base = 10, b32 uppercase = false) {
    assert(buffer_size >= 20);

    u8 *ptr = buffer + buffer_size;
    s32 written = 0;

    u8 *lookup = uppercase ? CharacterLookupUppercase : CharacterLookup;

    do {
        u64 quot = number / base;
        u64 rem  = number % base;

        ptr -= 1;
        *ptr = lookup[rem];
        written += 1;
        number = quot;
    } while (number);

    String result = {ptr, written};
    return result;
}


String format(Allocator alloc, String fmt, va_list args) {
    StringBuilder writer = {};
    DEFER(destroy(&writer));

    for (s64 i = 0; i < fmt.size; i += 1) {
        if (fmt[i] == '%' && i + 1 < fmt.size) {
            i += 1;

            switch (fmt[i]) {
            /* case 'd': { */
            /*     u8 buffer[128]; */
            /*     String ref = convert_signed_to_string(buffer, 128, va_arg(args, s32), 10, false, false); */
            /**/
            /*     file_buffer_append(&ConsoleFileBuffer, out, ref.data, ref.size); */
            /* } break; */
            /**/
            /* case 'u': { */
            /*     u8 buffer[128]; */
            /*     s32 length = convert_unsigned_to_string(buffer, 128, va_arg(args, u32), 10, false); */
            /**/
            /*     file_buffer_append(&ConsoleFileBuffer, out, buffer, length); */
            /* } break; */
            /**/
            /* case 'l': { */
            /*     if (fmt[1] == 'x') { */
            /*         u8 buffer[128]; */
            /*         String ref = convert_signed_to_string(buffer, 128, va_arg(args, s64), 16, false, false); */
            /**/
            /*         file_buffer_append(&ConsoleFileBuffer, out, ref.data, ref.size); */
            /*         break; */
            /*     } else { */
            /*         die("Unknown format specifier.\n"); */
            /*     } */
            /*     die("Unknown format specifier %l\n"); */
            /* } break; */
            /**/
            /* case 'x': { */
            /*     u8 buffer[128]; */
            /*     s32 length = convert_unsigned_to_string(buffer, 128, va_arg(args, u32), 16, false); */
            /**/
            /*     file_buffer_append(&ConsoleFileBuffer, out, buffer, length); */
            /* } break; */
            /**/
            /* case 'p': { */
            /*     u8 buffer[18]; */
            /*     convert_to_ptr_string(buffer, 18, va_arg(args, void*)); */
            /**/
            /*     file_buffer_append(&ConsoleFileBuffer, out, buffer, 18); */
            /* } break; */
            /**/
            /* case 'f': { */
            /*     u8 buffer[512]; */
            /*     String res = convert_double_to_string(buffer, 512, va_arg(args, r64), 6, false, false, false, false); */
            /**/
            /*     file_buffer_append(&ConsoleFileBuffer, out, res.data, res.size); */
            /* } break; */
            /**/
            /* case 'c': { */
            /*     u8 c = va_arg(args, int); */
            /*     file_buffer_append(&ConsoleFileBuffer, out, &c, 1); */
            /* } break; */
            /**/
            case 'd': {
                u32 const buffer_size = 32;
                u8 buffer[buffer_size];
                String ref = convert_signed_to_string(buffer, buffer_size, va_arg(args, s32));

                append(&writer, ref);
            } break;

            case 's': {
                char *str = va_arg(args, char *);
                s64 length = c_string_length(str);

                String tmp = {(u8*)str, length};
                append(&writer, tmp);
            } break;

            case 'S': {
                String str = va_arg(args, String);

                append(&writer, str);
            } break;
            }

            continue;
        }
        append(&writer, (u8)fmt[i]);
    }

    return to_allocated_string(&writer, alloc);
}

String format(Allocator alloc, String fmt, ...) {
    va_list args;
    va_start(args, fmt);
    String result = format(alloc, fmt, args);
    va_end(args);

    return result;
}

String format(String fmt, ...) {
    va_list args;
    va_start(args, fmt);
    String result = format(default_allocator(), fmt, args);
    va_end(args);

    return result;
}

String t_format(String fmt, ...) {
    va_list args;
    va_start(args, fmt);
    String result = format(temporary_allocator(), fmt, args);
    va_end(args);

    return result;
}

b32 flush_write_buffer(PlatformFile *file) {
    if (platform_write(file, file->write_buffer.memory, file->write_buffer.used) != file->write_buffer.used) return false;
    file->write_buffer.used = 0;

    return true;
}

s64 write(PlatformFile *file, void const *buffer, s64 size) {
    s64 written = 0;

    if (file->write_buffer.memory) {
        s64 space = file->write_buffer.alloc - file->write_buffer.used;
        if (space < size) {
            written += platform_write(file, file->write_buffer.memory, file->write_buffer.used);
            file->write_buffer.used = 0;

            written += platform_write(file, buffer, size);
        } else {
            copy_memory(file->write_buffer.memory + file->write_buffer.used, buffer, size);
            written += size;
        }
    } else {
        written = platform_write(file, buffer, size);
    }

    return written;
}

b32 write_byte(PlatformFile *file, u8 value) {
    if (file->write_buffer.memory) {
        if (file->write_buffer.used == file->write_buffer.alloc) {
            flush_write_buffer(file);
        }
        file->write_buffer.memory[file->write_buffer.used] = value;
        file->write_buffer.used += 1;

        if (value == '\n') {
            flush_write_buffer(file);
        }
    } else {
        if (platform_write(file, &value, 1) != 1) return false;
    }

    return true;
}

s64 print_internal(PlatformFile *file, String fmt, va_list args) {
    assert(file);

    if (!file->open) return 0;
    s64 written = 0;

    for (s64 i = 0; i < fmt.size; i += 1) {
        if (fmt[i] == '%') {
            if (i + 1 == fmt.size) {
                if (write_byte(file, '%')) written += 1;

                return written;
            }

            i += 1;
            switch (fmt[i]) {
            case 'd': {
                u32 const buffer_size = 32;
                u8 buffer[buffer_size];
                String ref = convert_signed_to_string(buffer, buffer_size, va_arg(args, s32));

                for (s64 j = 0; j < ref.size; j += 1) {
                    if (!write_byte(file, ref[j])) return written;
                    written += 1;
                }
            } break;

            case 'u': {
                u32 const buffer_size = 32;
                u8 buffer[buffer_size];
                String ref = convert_unsigned_to_string(buffer, buffer_size, va_arg(args, u32));

                for (s64 j = 0; j < ref.size; j += 1) {
                    if (!write_byte(file, ref[j])) return written;
                    written += 1;
                }
            } break;

            case 'U': {
                u32 const buffer_size = 32;
                u8 buffer[buffer_size];

                u64 value = va_arg(args, u64);
                String ref = convert_unsigned_to_string(buffer, buffer_size, value);

                for (s64 j = 0; j < ref.size; j += 1) {
                    if (!write_byte(file, ref[j])) return written;
                    written += 1;
                }
            } break;

            case 'p': {
                u32 const buffer_size = 32;
                u8 buffer[buffer_size];

                void *ptr = va_arg(args, void*);
                String ref = convert_unsigned_to_string(buffer, buffer_size, (u64)ptr, 16);

                while (ref.size < 16) {
                    ref.data -= 1;
                    ref.size += 1;
                    ref[0] = '0';
                }

                ref.data -= 2;
                ref.size += 2;
                ref[0] = '0';
                ref[1] = 'x';

                for (s64 j = 0; j < ref.size; j += 1) {
                    if (!write_byte(file, ref[j])) return written;
                    written += 1;
                }
            } break;

            /* case 'l': { */
            /*     if (fmt[1] == 'x') { */
            /*         u8 buffer[128]; */
            /*         String ref = convert_signed_to_string(buffer, 128, va_arg(args, s64), 16, false, false); */
            /**/
            /*         file_buffer_append(&ConsoleFileBuffer, out, ref.data, ref.size); */
            /*         break; */
            /*     } else { */
            /*         die("Unknown format specifier.\n"); */
            /*     } */
            /*     die("Unknown format specifier %l\n"); */
            /* } break; */
            /**/
            /* case 'x': { */
            /*     u8 buffer[128]; */
            /*     s32 length = convert_unsigned_to_string(buffer, 128, va_arg(args, u32), 16, false); */
            /**/
            /*     file_buffer_append(&ConsoleFileBuffer, out, buffer, length); */
            /* } break; */
            /**/
            /* case 'p': { */
            /*     u8 buffer[18]; */
            /*     convert_to_ptr_string(buffer, 18, va_arg(args, void*)); */
            /**/
            /*     file_buffer_append(&ConsoleFileBuffer, out, buffer, 18); */
            /* } break; */
            /**/
            /* case 'f': { */
            /*     u8 buffer[512]; */
            /*     String res = convert_double_to_string(buffer, 512, va_arg(args, r64), 6, false, false, false, false); */
            /**/
            /*     file_buffer_append(&ConsoleFileBuffer, out, res.data, res.size); */
            /* } break; */
            /**/
            /* case 'c': { */
            /*     u8 c = va_arg(args, int); */
            /*     file_buffer_append(&ConsoleFileBuffer, out, &c, 1); */
            /* } break; */
            /**/
            case 's': {
                char *str = va_arg(args, char *);

                for (s64 j = 0; str[j] != 0; j += 1) {
                    if (!write_byte(file, str[j])) return written;
                    written += 1;
                }
            } break;

            case 'S': {
                String str = va_arg(args, String);

                for (s64 j = 0; j < str.size; j += 1) {
                    if (!write_byte(file, str[j])) return written;
                    written += 1;
                }
            } break;
            }
        } else {
            if (!write_byte(file, fmt[i])) return written;
            written += 1;
        }
    }

    return written;
}

s64 print(String fmt, ...) {
    va_list args;
    va_start(args, fmt);

    s64 written = print_internal(&Console.out, fmt, args);

    va_end(args);

    return written;
}

s64 format(PlatformFile *file, String fmt, ...) {
    va_list args;
    va_start(args, fmt);

    s64 written = print_internal(file, fmt, args);

    va_end(args);

    return written;
}

s64 format(struct PlatformFile *file, String fmt, va_list args) {
    return print_internal(file, fmt, args);
}

INTERNAL PlatformFile *LogFile;
INTERNAL String LogModeLookup[] = {
    "",
    "Note: ",
    "Error: ",
};

void log(s32 line, String file, u32 mode, String fmt, ...) {
    if (LogFile == 0) return;

    va_list args;
    va_start(args, fmt);

    format(LogFile, "%S:%d: %S", file, line, LogModeLookup[mode]);
    format(LogFile, fmt, args);

    va_end(args);
}

void log_to_file(struct PlatformFile *file) {
    LogFile = file;
}

enum {
    READ_ENTIRE_FILE_OK,
    READ_ENTIRE_FILE_NOT_FOUND,
    READ_ENTIRE_FILE_READ_ERROR,
};
struct ReadEntireFileResult {
    s32 status;
    String content;
};

ReadEntireFileResult read_entire_file(String file);


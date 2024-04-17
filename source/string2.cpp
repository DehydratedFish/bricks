#include "definitions.h"
#include "utf.cpp"

#include <cstdarg>

INTERNAL String convert_signed_to_string(u8 *buffer, s32 buffer_size, s64 signed_number, s32 base = 10, b32 uppercase = false, b32 keep_sign = false);

struct CharResult {
    u8  character;
    b32 empty;
};

INTERNAL CharResult get(String *str) {
    if (str->size == 0) return {0, true};

    u8 result = str->data[0];
    str->data += 1;
    str->size -= 1;

    return {result};
}

INTERNAL CharResult peek(String *str) {
    if (str->size == 0) return {0, true};

    return {str->data[0]};
}

INTERNAL u8 last(String str) {
    if (str.size) return str[str.size - 1];

    return 0;
}

struct GetLineResult {
    String line;
    b32 empty;
};

INTERNAL GetLineResult get_text_line(String *str) {
    if (str == 0 || str->size == 0) return {{}, true};

    String line = *str;
    line.size = 0;

    for (CharResult c = get(str); !c.empty; c = get(str)) {
        if (c.character == '\r') {
            c = peek(str);
            if (c.character == '\n') {
                str->data += 1;
                str->size -= 1;
            }

            break;
        }
        if (c.character == '\n') break;

        line.size += 1;
    }

    return {line, false};
}

INTERNAL bool equal(String lhs, String rhs) {
    if (lhs.size == rhs.size) {
        for (s64 i = 0; i < lhs.size; i += 1) {
            if (lhs[i] != rhs[i]) return false;
        }

        return true;
    }

    return false;
}

INTERNAL u8 lower_char(u8 c) {
    if (c >= 'A' && c <= 'Z') {
        c |= 32;
    }

    return c;
}

INTERNAL bool equal_insensitive(String lhs, String rhs) {
    if (lhs.size == rhs.size) {
        for (s64 i = 0; i < lhs.size; i += 1) {
            if (lower_char(lhs[i]) != lower_char(rhs[i])) return false;
        }

        return true;
    }

    return false;
}

INTERNAL bool contains(String str, String search) {
    for (s64 i = 0; i < str.size; i += 1) {
        if (i + search.size > str.size) return false;

        String tmp = {str.data + i, search.size};
        if (equal(tmp, search)) return true;
    }

    return false;
}

INTERNAL String shrink_front(String str, s64 amount = 1) {
#ifdef BOUNDS_CHECKING
    if (str.size < amount) die("String is too small to shrink.");
#endif
    str.data += amount;
    str.size -= amount;

    return str;
}

INTERNAL String shrink_back(String str, s64 amount = 1) {
#ifdef BOUNDS_CHECKING
    if (str.size < amount) die("String is too small to shrink.");
#endif
    str.size -= amount;

    return str;
}

INTERNAL String shrink(String str, s64 amount = 1) {
#ifdef BOUNDS_CHECKING
    if (str.size < amount * 2) die("String is too small to shrink.");
#endif
    return shrink_back(shrink_front(str, amount), amount);
}

INTERNAL String trim(String str) {
    s64 num_whitespaces = 0;

    for (s64 i = 0; i < str.size; i += 1) {
        if (str[i] > ' ') break;

        num_whitespaces += 1;
    }
    str.data += num_whitespaces;
    str.size += num_whitespaces;

    num_whitespaces = 0;
    for (s64 i = str.size; i > 0; i -= 1) {
        if (str[i - 1] > ' ') break;

        num_whitespaces += 1;
    }
    str.size -= num_whitespaces;

    return str;
}

INTERNAL String path_without_filename(String path) {
    for (s64 i = path.size; i > 0; i -= 1) {
        if (path[i - 1] == '/' || path[i - 1] == '\\') {
            return {path.data, i - 1};
        }
    }

    return path;
}

INTERNAL String filename_without_path(String path) {
    for (s64 i = path.size; i > 0; i -= 1) {
        if (path[i - 1] == '/' || path[i - 1] == '\\') {
            return {path.data + i, path.size - i};
        }
    }

    return path;
}

INTERNAL String filename_without_extension(String path) {
    String file = filename_without_path(path);

    for (s64 i = file.size; i > 0; i -= 1) {
        s64 index = i - 1;

        if (file[index] == '.') {
            file.size = index;

            break;
        }
    }

    return file;
}

INTERNAL bool operator==(String lhs, String rhs) {
    return equal(lhs, rhs);
}

INTERNAL bool operator!=(String lhs, String rhs) {
    return !(equal(lhs, rhs));
}

INTERNAL Ordering compare(String lhs, String rhs) {
	for (s32 i = 0; i != lhs.size && i != rhs.size; i += 1) {
		if (lhs[i] < rhs[i]) return CMP_LESSER;
		if (lhs[i] > rhs[i]) return CMP_GREATER;
	}

	s32 order = lhs.size - rhs.size;
	if (order < 0) return CMP_LESSER;
	else if (order > 0) return CMP_GREATER;
	else return CMP_EQUAL;
}

INTERNAL b32 starts_with(String str, String begin) {
    if (str.size < begin.size) return false;

    str.size = begin.size;
    return str == begin;
}

INTERNAL bool ends_with(String str, String search) {
    if (str.size < search.size) return false;

    str.data += str.size - search.size;
    str.size  = search.size;

    return str == search;
}

INTERNAL s64 find_first(String str, u32 c) {
    s64 result = -1;

    for (UTF8Iterator it = make_utf8_it(str); it.valid; next(&it)) {
        if (it.cp == c) {
            result = it.index;
            break;
        }
    }

    return result;
}

INTERNAL Array<String> split(String text, u8 c, Allocator alloc = temporary_allocator()) {
    DArray<String> lines = {};
    lines.allocator = alloc;

    String *line = append(lines, {text.data, 0});

    for (s64 i = 0; i < text.size; i += 1) {
        if (text[i] == c) {
            line = append(lines, {text.data + i + 1, 0});
        } else {
            line->size += 1;
        }
    }

    return lines;
}

INTERNAL Array<String> split_by_line_ending(String text, Allocator alloc = temporary_allocator()) {
    DArray<String> lines = {};
    lines.allocator = alloc;

    String *line = append(lines, {text.data, 0});

    u32 const multi_char_line_end = '\n' + '\r';
    for (s64 i = 0; i < text.size; i += 1) {
        if (text[i] == '\n' || text[i] == '\r') {
            if (i + 1 < text.size && text[i] + text[i + 1] == multi_char_line_end) {
                i += 1;
            }

            line = append(lines, {text.data + i + 1, 0});
        } else {
            line->size += 1;
        }
    }

    return lines;
}

INTERNAL u8 read_byte(String buffer, s64 offset) {
#ifdef BOUNDS_CHECKING
    if (buffer.size < offset) die("String read out of bounds.");
#endif
    return buffer[offset];
}

INTERNAL u16 read_le_u16(String buffer, s64 offset) {
#ifdef BOUNDS_CHECKING
    if (buffer.size < offset + sizeof(u16)) die("String read out of bounds.");
#endif
    u8 *data = buffer.data + offset;

    return data[0] | (data[1] << 8);
}

INTERNAL u32 read_le_u32(String buffer, s64 offset) {
#ifdef BOUNDS_CHECKING
    if (buffer.size < offset + sizeof(u32)) die("String read out of bounds.");
#endif
    u8 *data = buffer.data + offset;

    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

INTERNAL s64 read_le_s64(String buffer, s64 offset) {
#ifdef BOUNDS_CHECKING
    if (buffer.size < offset + sizeof(s64)) die("String read out of bounds.");
#endif
    u8 *data = buffer.data + offset;

    s64 result = (data[0])            | (data[1] << 8)       | (data[2] << 16)      | (data[3] << 24) |
                 ((s64)data[4] << 32) | ((s64)data[5] << 40) | ((s64)data[6] << 48) | ((s64)data[7] << 56);

    return result;
}

INTERNAL r32 read_le_r32(String buffer, s64 offset) {
#ifdef BOUNDS_CHECKING
    if (buffer.size < offset + sizeof(r32)) die("String read out of bounds.");
#endif
    u32 tmp = read_le_u32(buffer, offset);

    return *(r32*)&tmp;
}

INTERNAL String sub_string(String buffer, s64 offset, s64 size) {
#ifdef BOUNDS_CHECKING
    if (buffer.size < offset + size) die("String read out of bounds.");
#endif
    String result = {buffer.data + offset, size};

    return result;
}

INTERNAL s32 write_le_u32(String buffer, s64 offset, u32 value) {
#ifdef BOUNDS_CHECKING
    if (buffer.size < offset + sizeof(u32)) die("String write out of bounds.");
#endif

    u8 *data = buffer.data + offset;
    data[0] = value & 0x000000FF;
    data[1] = (value & 0x0000FF00) >> 8;
    data[2] = (value & 0x00FF0000) >> 16;
    data[3] = (value & 0xFF000000) >> 24;

    return 4;
}

INTERNAL s32 write_le_s64(String buffer, s64 offset, s64 value) {
#ifdef BOUNDS_CHECKING
    if (buffer.size < offset + sizeof(s64)) die("String write out of bounds.");
#endif

    u8 *data = buffer.data + offset;
    data[0] = value & 0x000000FF;
    data[1] = (value & 0x0000FF00) >> 8;
    data[2] = (value & 0x00FF0000) >> 16;
    data[3] = (value & 0xFF000000) >> 24;
    data[4] = (value & 0xFF000000) >> 32;
    data[5] = (value & 0xFF000000) >> 40;
    data[6] = (value & 0xFF000000) >> 48;
    data[7] = (value & 0xFF000000) >> 56;

    return 8;
}

INTERNAL s32 write_string_with_size(String buffer, s64 offset, String data) {
    s32 size = data.size;

#ifdef BOUNDS_CHECKING
    if (buffer.size < offset + data.size + sizeof(size)) die("String write out of bounds.");
#endif

    write_le_u32(buffer, offset, size);
    copy_memory(buffer.data + offset + sizeof(size), data.data, data.size);

    return sizeof(size) + data.size;
}

INTERNAL s32 write_raw(String buffer, s64 offset, String data) {
#ifdef BOUNDS_CHECKING
    if (buffer.size < offset + data.size) die("String write out of bounds.");
#endif

    copy_memory(buffer.data + offset, data.data, data.size);

    return data.size;
}

// NOTE: simple djb2 hash
INTERNAL u32 string_hash(String string) {
	u32 hash = 5381;

	for (s32 i = 0; i < string.size; i += 1) {
		hash = hash * 33 ^ string[i];
	}

	return hash;
}

INTERNAL String allocate_string(Allocator alloc, s64 size) {
    String result = {};
    result.data = ALLOC(alloc, u8, size);
    result.size = size;

    return result;
}

INTERNAL String allocate_string(s64 size) {
    return allocate_string(default_allocator(), size);
}

INTERNAL String allocate_temp_string(s64 size) {
    String result = {};
    result.data = (u8*)allocate(temporary_allocator(), size);
    result.size = size;

    return result;
}

INTERNAL String allocate_string(Allocator alloc, String str) {
    String result = allocate_string(alloc, str.size);

    copy_memory(result.data, str.data, str.size);

    return result;
}

INTERNAL String allocate_string(String str) {
    String result = allocate_string(default_allocator(), str.size);

    copy_memory(result.data, str.data, str.size);

    return result;
}

INTERNAL String allocate_temp_string(String str) {
    String result = allocate_temp_string(str.size);

    copy_memory(result.data, str.data, str.size);

    return result;
}

INTERNAL void destroy_string(String *str) {
    DEALLOC(default_allocator(), str->data, str->size);

    INIT_STRUCT(str);
}


INTERNAL void reset(StringBuilder *builder) {
    StringBuilderBlock *block = &builder->first;

    while (block) {
        block->used = 0;
        block = block->next;
    }

    builder->total_size = 0;
}

INTERNAL void append(StringBuilder *builder, u8 c, Allocator alloc = default_allocator()) {
    if (builder->allocator.allocate == 0) builder->allocator = alloc;

    if (builder->current == 0) builder->current = &builder->first;

    if (builder->current->used + 1 == STRING_BUILDER_BLOCK_SIZE) {
        if (builder->current->next == 0) {
            builder->current->next = ALLOC(builder->allocator, StringBuilderBlock, 1);
        }
        builder->current = builder->current->next;
    }

    builder->current->buffer[builder->current->used] = c;
    builder->current->used += 1;
    builder->total_size    += 1;
}

INTERNAL void append(StringBuilder *builder, String str) {
    if (builder->current == 0) builder->current = &builder->first;
    if (builder->allocator.allocate == 0) builder->allocator = default_allocator();

    s64 space = STRING_BUILDER_BLOCK_SIZE - builder->current->used;
    while (space < str.size) {
        copy_memory(builder->current->buffer + builder->current->used, str.data, space);
        builder->current->used += space;
        builder->total_size    += space;
        str = shrink_front(str, space);

        if (builder->current->next == 0) {
            builder->current->next = ALLOC(builder->allocator, StringBuilderBlock, 1);
        }
        builder->current = builder->current->next;

        space = STRING_BUILDER_BLOCK_SIZE;
    }

    copy_memory(builder->current->buffer + builder->current->used, str.data, str.size);
    builder->current->used += str.size;
    builder->total_size    += str.size;
}

INTERNAL void append_format(StringBuilder *builder, char const *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    while (fmt[0]) {
        if (fmt[0] == '%' && fmt[1]) {
            fmt += 1;

            switch (fmt[0]) {
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

                append(builder, ref);
            } break;

            case 's': {
                char *str = va_arg(args, char *);
                s64 length = c_string_length(str);

                String tmp = {(u8*)str, length};
                append(builder, tmp);
            } break;

            case 'S': {
                String str = va_arg(args, String);

                append(builder, str);
            } break;
            }

            fmt += 1;
            continue;
        }
        append(builder, (u8)fmt[0]);
        fmt += 1;
    }

    va_end(args);
}

INTERNAL void destroy(StringBuilder *builder) {
    StringBuilderBlock *next = builder->first.next;

    while (next) {
        StringBuilderBlock *tmp = next->next;
        DEALLOC(builder->allocator, next, 1);

        next = tmp;
    }
}

INTERNAL String to_allocated_string(StringBuilder *builder, Allocator alloc = default_allocator()) {
    String result = allocate_string(alloc, builder->total_size);

    s64 size = 0;
    StringBuilderBlock *block = &builder->first;
    while (block) {
        copy_memory(result.data + size, block->buffer, block->used);
        size += block->used;

        block = block->next;
    }

    return result;
}

INTERNAL String temp_string(StringBuilder *builder) {
    assert(builder->total_size < STRING_BUILDER_BLOCK_SIZE);

    return {builder->first.buffer, builder->first.used};
}


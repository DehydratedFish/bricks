#include "bricks.h"

struct UTF16GetResult {
    u16 value[2];
    b32 is_surrogate;
    b32 status;
};

INTERNAL UTF16GetResult get_utf16(String16 *string) {
    if (string->size == 0) return {{}, 0, GET_EMPTY};
    if (string->data[0] > 0xDBFF && string->data[0] < 0xE000) return {{}, 0, GET_ERROR};

    UTF16GetResult result = {};
    if (string->data[0] > 0xD7FF && string->data[0] < 0xDC00) {
        if (string->size < 2) return {{}, 0, GET_ERROR};

        result.value[0] = string->data[0];
        result.value[1] = string->data[1];
        result.is_surrogate = true;
        result.status   = GET_OK;

        string->data += 2;
        string->size -= 2;
    } else {
        result.value[0] = string->data[0];
        result.is_surrogate = false;
        result.status   = GET_OK;

        string->data += 1;
        string->size -= 1;
    }

    return result;
}

String to_utf8(Allocator alloc, String16 string) {
    if (string.size == 0) return {};

    DArray<u8> buffer = {};
    buffer.allocator = alloc;

    UTF16GetResult c = get_utf16(&string);
    while (c.status == GET_OK) {
        if (c.is_surrogate) {
            u32 cp = ((c.value[0] - 0xD800) >> 10) + (c.value[1] - 0xDC00) + 0x10000;

            u8 byte;
            byte = 0xF0 | (cp >> 18);
            append(buffer, byte);
            byte = 0x80 | ((cp >> 12) & 0x3F);
            append(buffer, byte);
            byte = 0x80 | ((cp >> 6) & 0x3F);
            append(buffer, byte);
            byte = 0x80 | (cp & 0x3F);
            append(buffer, byte);
        } else {
            u16 cp = c.value[0];

            u8 byte;
            if (cp < 0x80) {
                byte = cp;
                append(buffer, byte);
            } else if (cp < 0x800) {
                byte = 0xC0 | (cp >> 6);
                append(buffer, byte);
                byte = 0x80 | (cp & 0x3F);
                append(buffer, byte);
            } else {
                byte = 0xE0 | (cp >> 12);
                append(buffer, byte);
                byte = 0x80 | ((cp >> 6) & 0x3F);
                append(buffer, byte);
                byte = 0x80 | (cp & 0x3F);
                append(buffer, byte);
            }
        }

        c = get_utf16(&string);
    }

    if (c.status == GET_ERROR) {
        die("Malformed utf16 string.");
    }

    return {buffer.memory, buffer.size};
}

INTERNAL s32 utf8_length(u8 c) {
    if      ((c & 0x80) == 0x00) return 1;
    else if ((c & 0xE0) == 0xC0) return 2;
    else if ((c & 0xF0) == 0xE0) return 3;
    else if ((c & 0xF8) == 0xF0) return 4;

    return -1;
}

// TODO: Maybe lookup table?
INTERNAL s32 utf8_length(String str) {
    if (str.size == 0) return 0;

    return utf8_length(str[0]);
}

struct UTF8GetResult {
    u8  byte[4];
    s32 size;
    b32 status;
};

INTERNAL UTF8GetResult get_utf8(String *string) {
    UTF8GetResult result = {};

    s32 size = utf8_length(*string);
    if (size == -1) {
        result.status = GET_ERROR;

        return result;
    }
    if (size == 0) {
        result.status = GET_EMPTY;
    }

    copy_memory(result.byte, string->data, size);
    result.size = size;

    string->data += size;
    string->size -= size;
    
    return result;
}

/*
 * Note: Helpful stackoverflow question
 * https://stackoverflow.com/questions/73758747/looking-for-the-description-of-the-algorithm-to-convert-utf8-to-utf16
 */
String16 to_utf16(Allocator alloc, String string, b32 add_null_terminator = false) {
    if (string.size == 0) return {};

    DArray<u16> buffer = {};
    buffer.allocator = alloc;

    UTF8GetResult c = get_utf8(&string);
    while (c.status == GET_OK) {
        if (c.size == 1) {
            u16 value = c.byte[0] & 0x7F;
            append(buffer, value);
        } else if (c.size == 2) {
            u16 value = ((c.byte[0] & 0x1F) << 6) | (c.byte[1] & 0x3F);
            append(buffer, value);
        } else if (c.size == 3) {
            u16 value = ((c.byte[0] & 0x0F) << 12) | ((c.byte[1] & 0x3F) << 6) | (c.byte[2] & 0x3F);
            append(buffer, value);
        } else if (c.size == 4) {
            u32 cp = ((c.byte[0] & 0x07) << 18) | ((c.byte[1] & 0x3F) << 12) | ((c.byte[2] & 0x3F) << 6) | (c.byte[3] & 0x3F);
            cp -= 0x10000;

            u16 high = 0xD800 + ((cp >> 10) & 0x3FF);
            u16 low  = 0xDC00 + (cp & 0x3FF);

            append(buffer, high);
            append(buffer, low );
        }

        c = get_utf8(&string);
    }

    if (c.status == GET_ERROR) {
        // TODO: better error handling
        die("Malformed utf8");
    }

    if (add_null_terminator) {
        u16 n = L'\0';
        append(buffer, n);
    }

    return {buffer.memory, buffer.size};
}

void next(UTF8Iterator *it) {
    assert(it);

    UTF8GetResult c = get_utf8(&it->string);
    if (c.status != GET_OK) {
        INIT_STRUCT(it);

        return;
    }

    if (c.size == 1) {
        it->cp = c.byte[0] & 0x7F;
    } else if (c.size == 2) {
        it->cp = ((c.byte[0] & 0x1F) << 6) | (c.byte[1] & 0x3F);
    } else if (c.size == 3) {
        it->cp = ((c.byte[0] & 0x0F) << 12) | ((c.byte[1] & 0x3F) << 6) | (c.byte[2] & 0x3F);
    } else if (c.size == 4) {
        it->cp = ((c.byte[0] & 0x07) << 18) | ((c.byte[1] & 0x3F) << 12) | ((c.byte[2] & 0x3F) << 6) | (c.byte[3] & 0x3F);
    }
    it->index += c.size;
    it->valid = true;
}

UTF8Iterator make_utf8_it(String string) {
    UTF8Iterator it = {};
    it.string = string;
    next(&it);
    it.index = 0;

    return it;
}

UTF8CharResult utf8_peek(String str) {
    UTF8CharResult result = {};

    if (str.size == 0) {
        result.status = GET_EMPTY;

        return result;
    }

    s32 length = utf8_length(str.data[0]);
    if (str.size < length) {
        result.status = GET_ERROR;

        return result;
    }

    if (length == 1) {
        result.cp = str[0] & 0x7F;
    } else if (length == 2) {
        result.cp = ((str[0] & 0x1F) << 6) | (str[1] & 0x3F);
    } else if (length == 3) {
        result.cp = ((str[0] & 0x0F) << 12) | ((str[1] & 0x3F) << 6) | (str[2] & 0x3F);
    } else if (length == 4) {
        result.cp = ((str[0] & 0x07) << 18) | ((str[1] & 0x3F) << 12) | ((str[2] & 0x3F) << 6) | (str[3] & 0x3F);
    }

    copy_memory(result.byte, str.data, length);
    result.length = length;
    result.status = GET_OK;

    return result;
}


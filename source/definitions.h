#pragma once

#include <cstdint>


#if defined(WIN32) || defined(_WIN32)
#define OS_WINDOWS
#elif defined(linux)
#define OS_LINUX
#else
#error "Platform not supported"
#endif

void fire_assert(char const *msg, char const *func, char const *file, int line); // NOTE: defined in platform.c


#ifdef DEVELOPER
#define assert(expr) (void)((expr) || (fire_assert(#expr, __func__, __FILE__, __LINE__),0))

#else
#define assert(expr)

#endif // DEVELOPER

#ifdef BOUNDS_CHECKING
#define BOUNDS_CHECK(low, high, index, msg) {if ((index) < (low) || (index) > (high)) fire_assert(msg, __func__, __FILE__, __LINE__);}

#else
#define BOUNDS_CHECK(low, high, index, msg)

#endif // BOUNDS_CHECKING


#define INTERNAL static

#define KILOBYTES(x) ((x) * 1024)
#define MEGABYTES(x) (KILOBYTES(x) * 1024)
#define GIGABYTES(x) (MEGABYTES(x) * 1024)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

#define STRUCT_OFFSET(type, member) ((u64)&(((type*)0)->member))
#define STRUCT_OFFSET_PTR(type, member) (&(((type*)0)->member))

#define PACK_RGBA(r, g, b, a) (r | (g << 8) | (b << 16) | (a << 24))
#define PACK_RGB(r, g, b) PACK_RGBA(r, g, b, 255)


typedef enum Ordering {
    CMP_LESSER  = -1,
    CMP_EQUAL   =  0,
    CMP_GREATER =  1
} Ordering;

#define PI 3.14159265358979

typedef int8_t  s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef float  r32;
typedef double r64;

typedef s8  b8;
typedef s32 b32;

typedef void VoidFunc(void);


typedef void*(AllocatorFunc)(void *data, s64 size, void *old, s64 old_size);
struct Allocator {
    AllocatorFunc *allocate;
    void *data;
};

struct MemoryArena {
    Allocator allocator;

    u8 *memory;
    s64 used;
    s64 alloc;

    s64 all_time_high;
};

struct MemoryBuffer {
    u8 *memory;
    s64 used;
    s64 alloc;
};


inline constexpr s64 c_string_length(char const *str) {
	if (str == 0) return 0;

	s64 size = 0;
	for (; str[size]; size += 1);

	return size;
}

struct String {
	u8 *data;
	s64 size;

	String() = default;
	String(u8 *buffer, s64 length) {
		data = buffer;
		size = length;
	}

        //NOTE: we never touch a reference and always copy before a change so this cast should be fine
	constexpr String(char const *str)
            : data((u8*)str), size(c_string_length(str)) {}

	String &operator=(char const *str) {
		data = (u8*)str;
		size = c_string_length(str);

		return *this;
	}

	u8 &operator[](s64 index) {
		BOUNDS_CHECK(0, size - 1, index, "String indexing out of bounds");

		return data[index];
	}

};

#define FOR(array, it_name) for (auto *it_name = (array).memory; it_name != (array).memory + (array).size; it_name += 1)

template<typename Type>
struct Array {
    Type *memory;
    s64   size;

    Type &operator[](s64 index) {
        BOUNDS_CHECK(0, size - 1, index, "Array indexing out of bounds");

        return memory[index];
    }

    Type const &operator[](s64 index) const {
        BOUNDS_CHECK(0, size - 1, index, "Array indexing out of bounds");

        return memory[index];
    }
};

template<typename Type>
struct DArray {
    Allocator allocator;

    Type *memory;
    s64   size;
    s64   alloc;

    Type &operator[](u64 index) {
        BOUNDS_CHECK(0, size - 1, index, "Array indexing out of bounds");

        return memory[index];
    }

    Type const &operator[](u64 index) const {
        BOUNDS_CHECK(0, size - 1, index, "Array indexing out of bounds");

        return memory[index];
    }

    operator Array<Type>() {
        return {memory, size};
    }
};

struct V2 {
    union {
        struct {
            r32 x;
            r32 y;
        };
        struct {
            r32 width;
            r32 height;
        };
        r32 value[2];
    };
};

struct V3 {
    union {
        struct {
            r32 x;
            r32 y;
            r32 z;
        };
        r32 value[3];
    };
};

struct V4 {
    union {
        struct {
            r32 x;
            r32 y;
            r32 z;
            r32 w;
        };
        struct {
            r32 r;
            r32 g;
            r32 b;
            r32 a;
        };
        r32 value[4];
    };
};
typedef V4 Color;

r32 const ColorComponentFactor = 1.0f / 255.0f;

struct V2i {
    union {
        struct {
            s32 x;
            s32 y;
        };
    };
};

struct M3 {
    r32 value[9];
};

struct M4 {
    r32 value[16];
};

enum {
    GET_OK    = 0x00,
    GET_EMPTY = 0x01,
    GET_ERROR = 0x02,
};

struct String16 {
    u16 *data;
    s64  size;
};

struct UTF8Iterator {
    String string;

    s64 index;
    u32 cp;
    b32 valid;
};

struct UTF8CharResult {
    u32 cp;
    u8  byte[4];
    s32 length;
    u32 status;
};


void die(String msg);
s64 convert_string_to_s64(u8 *buffer, s32 buffer_size);


#ifndef STRING_BUILDER_BLOCK_SIZE
#define STRING_BUILDER_BLOCK_SIZE KILOBYTES(4)
#endif

struct StringBuilderBlock {
    StringBuilderBlock *next;

    u8 buffer[STRING_BUILDER_BLOCK_SIZE];
    s64 used;
};
struct StringBuilder {
    Allocator allocator;

    StringBuilderBlock  first;
    StringBuilderBlock *current;

    s64 total_size;
};


template<typename Functor>
struct DeferGuardBase {
    Functor functor;

    DeferGuardBase(Functor f): functor(f) {}
    ~DeferGuardBase() {functor();}
};

template<typename Functor>
DeferGuardBase<Functor> make_defer_guard_base(Functor f) {return f;}

#define CONCAT(l, r) CONCAT2(l, r)
#define CONCAT2(l, r) l ## r

#define UNIQUE_NAME(base) CONCAT(base, __COUNTER__)

#define DEFER(stuff) DEFER2(UNIQUE_NAME(DeferGuardUniqueName), stuff)
#define DEFER2(name, stuff) auto name = make_defer_guard_base([&](){stuff;});

#define RESET_TEMP_STORAGE_ON_EXIT() RESET_TEMP_STORAGE_ON_EXIT2(CONCAT(TMP_STORAGE_REWIND_MARK, __COUNTER__))
#define RESET_TEMP_STORAGE_ON_EXIT2(name) auto name = temporary_storage_mark(); DEFER(temporary_storage_rewind(name));

#define CHANGE_AND_RESET_DEFAULT_ALLOCATOR(alloc) CHANGE_AND_RESET_DEFAULT_ALLOCATOR2(CONCAT(DEFAULT_ALLOCATOR_REWIND_MARK, __COUNTER__), alloc)
#define CHANGE_AND_RESET_DEFAULT_ALLOCATOR2(name, alloc) auto name = default_allocator(); DEFER(get_toolbox()->default_allocator = name);


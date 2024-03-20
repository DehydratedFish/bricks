#pragma once

#include "bricks.h"


INTERNAL void *allocate(Allocator allocator, s64 bytes) {
    return allocator.allocate(allocator.data, bytes, 0, 0);
}
INTERNAL void *reallocate(Allocator allocator, s64 bytes, void *old, s64 old_bytes) {
    return allocator.allocate(allocator.data, bytes, old, old_bytes);
}
INTERNAL void deallocate(Allocator allocator, void *ptr, s64 old_size) {
    if (!ptr) return;

    allocator.allocate(allocator.data, 0, ptr, old_size);
}


INTERNAL void zero_memory(void *data, s64 size) {
    u8 *ptr = (u8*)data;

    for (s64 i = 0; i < size; i += 1) ptr[i] = 0;
}

INTERNAL void copy_memory(void *dest, void const *src, s64 size) {
    u8 *d = (u8*)dest;
    u8 *s = (u8*)src;

    if (d < s) {
        for (s64 i = 0; i < size; i += 1) d[i] = s[i];
    } else {
        for (s64 i = size - 1; i > -1; i -= 1) d[i] = s[i];
    }
}

INTERNAL bool memory_is_equal(void const *lhs, void const *rhs, u64 size) {
    u8 *l = (u8*)lhs;
    u8 *r = (u8*)rhs;

    for (s64 i = 0; i < size; i += 1) {
        if (l[i] != r[i]) return false;
    }

    return true;
}

INTERNAL MemoryArena allocate_arena(s64 size) {
    MemoryArena arena = {};
    arena.allocator  = default_allocator();
    arena.memory = ALLOC(arena.allocator, u8, size);
    arena.used   = 0;
    arena.alloc  = size;

    return arena;
}

INTERNAL void destroy(MemoryArena *arena) {
    DEALLOC(arena->allocator, arena->memory, arena->alloc);

    INIT_STRUCT(arena);
}

INTERNAL void *allocate_from_arena(MemoryArena *arena, s64 size, void *old, s64 old_size) {
    u32 const alignment = alignof(void*);

    u8 *current_ptr = arena->memory + arena->used;

    if (old) {
        if (size) {
            if (current_ptr - old_size == old) {
                s64 additional_size = size - old_size;
                if (arena->used + additional_size > arena->alloc) die("Could not allocate from arena.");

                arena->used += additional_size;
                
                if (arena->used > arena->all_time_high) arena->all_time_high = arena->used;
                return old;
            } else {
                s64 space = arena->alloc - arena->used;

#pragma warning( suppress : 4146 )
                s64 padding = -(u64)current_ptr & (alignment - 1);

                if (size > (space - padding)) die("Could not allocate from arena.");

                void *result = arena->memory + padding + arena->used;
                zero_memory(result, size);
                arena->used += padding + size;

                copy_memory(result, old, old_size);

                if (arena->used > arena->all_time_high) arena->all_time_high = arena->used;
                return result;
            }
        } else {
            if (current_ptr - old_size == old) {
                arena->used -= old_size;
            }

            return 0;
        }
    }

    s64 space = arena->alloc - arena->used;

#pragma warning( suppress : 4146 )
    s64 padding = -(u64)current_ptr & (alignment - 1);

    if (size > (space - padding)) die("Could not allocate from arena.");

    void *result = arena->memory + padding + arena->used;
    zero_memory(result, size);
    arena->used += padding + size;

    if (arena->used > arena->all_time_high) arena->all_time_high = arena->used;
    return result;
}

INTERNAL Allocator make_arena_allocator(MemoryArena *arena) {
    return {(AllocatorFunc*)allocate_from_arena, arena};
}

#define ALLOCATE(arena, type, count) (type*)allocate_from_arena((arena), sizeof(type) * (count), 0, 0)

template<typename Type>
INTERNAL Array<Type> allocate_array(s64 size) {
    Array<Type> array = {};
    array.memory = ALLOC(get_toolbox()->default_allocator, Type, size);
    array.size   = size;

    return array;
}

template<typename Type>
INTERNAL void destroy_array(Array<Type> *array) {
    DEALLOC(get_toolbox()->default_allocator, array->memory, array->size);

    INIT_STRUCT(array);
}

template<typename Type>
INTERNAL void prealloc(Array<Type> &array, s64 size, Allocator alloc) {
    Type *memory = (Type*)allocate(alloc, size * sizeof(Type));
    copy_memory(memory, array.memory, array.size);

    array.memory = memory;
    array.size   = size;
}

template<typename Type>
INTERNAL void prealloc(DArray<Type> &array, MemoryArena *arena, s64 size) {
    Type *memory = ALLOCATE(arena, Type, size);
    copy_memory(memory, array.memory, array.size);

    array.memory = memory;
    array.alloc  = size;
}

template<typename Type>
INTERNAL void init(DArray<Type> &array, s64 size) {
    array.allocator = get_toolbox()->default_allocator;

    array.memory = (Type*)allocate(array.allocator, size * sizeof(Type));
    array.size   = 0;
    array.alloc  = size;
}

template<typename Type>
INTERNAL void prealloc(DArray<Type> &array, s64 size) {
    init(array, size);
    array.size = size;
}

template<typename Type>
INTERNAL Type *last(DArray<Type> &array) {
    if (array.size) return &array[array.size - 1];

    return 0;
}

template<typename Type>
INTERNAL Type *append(DArray<Type> &array, Type element) {
    if (array.allocator.allocate == 0) array.allocator = get_toolbox()->default_allocator;

    if (array.alloc == 0) {
        u32 const default_size = 4;
        array.memory = ALLOC(array.allocator, Type, default_size);
        array.alloc = default_size;
    } else if (array.size == array.alloc) {
        s64 new_alloc = array.alloc * 2;
        array.memory  = REALLOC(array.allocator, array.memory, array.alloc, new_alloc);
        array.alloc   = new_alloc;
    }

    array.memory[array.size] = element;
    array.size += 1;

    return &array.memory[array.size - 1];
}

template<typename Type>
INTERNAL Type *append_unique(DArray<Type> &array, Type element) {
    FOR (array, elem) {
        if (*elem == element) return elem;
    }

    return append(array, element);
}


template<typename Type>
INTERNAL Type *insert(DArray<Type> &array, s64 index, Type element) {
	BOUNDS_CHECK(0, array.size, index, "Array insertion out of bounds.");

    if (array.allocator.allocate == 0) array.allocator = get_toolbox()->default_allocator;

    if (array.alloc == 0) {
        u32 const default_size = 4;
        array.memory = ALLOC(array.allocator, Type, default_size);
        array.alloc = default_size;
    } else if (array.size == array.alloc) {
        s64 new_alloc = array.alloc * 2;
        array.memory  = REALLOC(array.allocator, array.memory, array.alloc, new_alloc);
        array.alloc   = new_alloc;
    }

    copy_memory(array.memory + index + 1, array.memory + index, (array.size - index) * sizeof(Type));
    array.memory[index] = element;
    array.size += 1;

    return &array.memory[index];
}

template<typename Type>
INTERNAL void append(DArray<Type> &array, Type *elements, s64 count) {
    if (array.allocator.allocate == 0) array.allocator = get_toolbox()->default_allocator;

    s64 space = array.alloc - array.size;
    if (space < count) {
        array.memory = REALLOC(array.allocator, array.memory, array.alloc, array.size + count);
        array.alloc  = array.size + count;
    }

    copy_memory(array.memory + array.size, elements, count * sizeof(Type));
    array.size += count;
}

template<typename Type>
INTERNAL void destroy(DArray<Type> &array) {
    if (!array.allocator.allocate) return;

    deallocate(array.allocator, array.memory, array.alloc * sizeof(Type));

    array.memory = 0;
    array.size   = 0;
    array.alloc  = 0;
}


INTERNAL MemoryBuffer allocate_buffer(MemoryArena *arena, s64 size) {
    MemoryBuffer buffer = {};
    buffer.memory = (u8*)ALLOCATE(arena, u8, size);
    buffer.used   = 0;
    buffer.alloc  = size;

    return buffer;
}


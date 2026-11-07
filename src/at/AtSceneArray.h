// Private to the scene's sources. The original's arrays of objects come from new[]: a count in the word ahead of the first element, and destroying goes by that count, last first. It frees without destroying when the count is 0, there being
// no element to ask. These write that out, since a heap here can answer null and new[] may not. One word is 4 bytes on the Xbox.
#pragma once

typedef decltype(sizeof(0)) AtSize;

struct AtPlacement {};
inline void* operator new(AtSize, void* where, AtPlacement) { return where; }
inline void operator delete(void*, void*, AtPlacement) {}

// Null if there was no memory. The heap's push and pop are the caller's.
template <class T> inline T* AtNewArray(unsigned int count) {
    AtSize* block = (AtSize*)operator new(count * sizeof(T) + sizeof(AtSize));
    if (block == 0) {
        return 0;
    }
    *block = count;
    T* first = (T*)(block + 1);
    for (unsigned int i = 0; i < count; i++) {
        new (first + i, AtPlacement()) T;
    }
    return first;
}

// One object from the current heap, or null. new may be assumed never to answer null.
template <class T> inline T* AtNewObject() {
    void* block = operator new(sizeof(T));
    return block != 0 ? new (block, AtPlacement()) T : 0;
}

template <class T> inline void AtDeleteArray(T* first) {
    AtSize* block = (AtSize*)first - 1;
    for (AtSize i = *block; i > 0; i--) {
        first[i - 1].~T();
    }
    operator delete(block);
}

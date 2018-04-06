/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information.
 *
 *    Copyright 2018 (c) Fraunhofer IOSB (Author: Julius Pfrommer)
 */

#include "ua_config.h"
#include "ua_bufmalloc.h"

#define MALLOCMEMBUFSIZE 16384

/* Global method pointers */
void * (*globalMalloc)(size_t size) = malloc;
void (*globalFree)(void *ptr) = free;
void * (*globalCalloc)(size_t nelem, size_t elsize) = calloc;
void * (*globalRealloc)(void *ptr, size_t size) = realloc;

/* Every element has the memory layout [length (size_t) | buf (length * sizeof(char)) ... ].
 * The pointer to buf is returned. */
static char membuf[MALLOCMEMBUFSIZE];
static size_t pos;

static void * membufMalloc(size_t size) {
    if(pos + size + sizeof(size_t) > MALLOCMEMBUFSIZE)
        return NULL;
    char *begin = &membuf[pos];
    *((size_t*)begin) = size;
    pos += size + sizeof(size_t);
    return &begin[sizeof(size_t)];
}

static void membufFree(void *ptr) {
    /* Don't do anyting */
}

static void * membufCalloc(size_t nelem, size_t elsize) {
    size_t total = nelem * elsize;
    void *mem = membufMalloc(total);
    if(!mem)
        return NULL;
    memset(mem, 0, total);
    return mem;
}

static void * (membufRealloc)(void *ptr, size_t size) {
    size_t orig_size = ((size_t*)ptr)[-1];
    if(size <= orig_size)
        return ptr;
    void *mem = membufMalloc(size);
    if(!mem)
        return NULL;
    memcpy(mem, ptr, orig_size);
    return mem;
}

void resetMembuf(void) {
    pos = 0;
}

void useMembufAlloc(void) {
    pos = 0;
    globalMalloc = membufMalloc;
    globalFree = membufFree;
    globalCalloc = membufCalloc;
    globalRealloc = membufRealloc;
}

void useNormalAlloc(void) {
    globalMalloc = malloc;
    globalFree = free;
    globalCalloc = calloc;
    globalRealloc = realloc;
}

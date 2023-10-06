#include "core/pstring.h"
#include "core/pmemory.h"

#include <string.h>

u64
string_length(const char* string) {
    return strlen(string);
}

char*
string_duplicate(const char* string) {
    u64 length  = string_length(string);
    char* copy = pallocate(length+1, MEMORY_TAG_STRING);
    pcopy_memory(copy, string, length+1);
    return copy;
}

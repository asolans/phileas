#include "complete.h"

#include <string.h>

size_t complete_word(char *word, size_t size, const char *const *names, size_t n)
{
    size_t len = strlen(word), common = 0, matches = 0;
    const char *first = NULL;

    for (size_t i = 0; i < n; i++) {
        if (strncmp(names[i], word, len) != 0)
            continue;
        if (!first) {
            first = names[i];
            common = strlen(first);
        } else {
            size_t k = len;

            while (k < common && names[i][k] == first[k])
                k++;
            common = k;
        }
        matches++;
    }
    if (first && common > len && common < size) {
        memcpy(word, first, common);
        word[common] = '\0';
    }
    return matches;
}

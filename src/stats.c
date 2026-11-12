#include "stats.h"
#include "utf8.h"

#include <wctype.h>

static int is_end(wchar_t c)
{
    return c == L'.' || c == L'!' || c == L'?' || c == L'…';
}

static int is_close(wchar_t c)
{
    return c == L'»' || c == L'”' || c == L'"' || c == L')' || c == L']' || c == L'’' || c == L'\'' || c == L'*' || c == L'_';
}

void doc_stats(const char *text, size_t len, DocStats *s)
{
    size_t i = 0;
    int in_word = 0, word_ok = 0, para_has = 0, para_open_sentence = 0, heading = 0, first = 1;

    *s = (DocStats){ 0 };
    while (i <= len) {
        wchar_t c = L'\n';
        size_t n = 1;

        if (i < len) {
            n = utf8_decode(text, len, i, &c);
            if (c == UTF8_INVALID)
                c = L'?', n = 1;
            if (!n)
                n = 1;
        }
        if (c == L'\n') {
            if (in_word && word_ok)
                s->words++;
            in_word = word_ok = 0;
            if (para_has) {
                s->paragraphs++;
                if (para_open_sentence && !heading)     /* un títol no és una frase */
                    s->sentences++;
            }
            para_has = para_open_sentence = heading = 0;
            first = 1;
            i += n;
            continue;
        }
        s->chars++;
        if (first && c == L'#')
            heading = 1;
        if (c != L' ' && c != L'\t')
            first = 0;
        if (!iswspace((wint_t)c))
            s->chars_nospace++;
        if (iswspace((wint_t)c)) {
            if (in_word && word_ok)
                s->words++;
            in_word = word_ok = 0;
        } else {
            in_word = 1;
            if (iswalnum((wint_t)c))
                word_ok = 1;
        }
        if (iswalnum((wint_t)c)) {
            para_has = 1;
            para_open_sentence = 1;
        }
        /* Fi de frase: ., !, ? o … (en una tirada, un de sol) seguits d'espai, tancaments
         * i fi de paràgraf; «3.5» o «a.b» no */
        if (is_end(c) && para_open_sentence) {
            size_t j = i + n;
            wchar_t d;
            size_t m;

            while (j < len) {
                m = utf8_decode(text, len, j, &d);
                if (d == UTF8_INVALID || !m || !(is_end(d) || is_close(d)))
                    break;
                j += m;
            }
            if (j >= len || text[j] == ' ' || text[j] == '\t' || text[j] == '\n') {
                s->sentences++;
                para_open_sentence = 0;
            }
        }
        i += n;
    }
    s->pages = (double)s->chars / STATS_PAGE_CHARS;
    s->minutes = (double)s->words / STATS_WPM;
}

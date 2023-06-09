#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

static char *file_get_content(const char *filename)
{
    FILE *fp;
    if (filename[0] == '-' && filename[1] == '\0') {
        fp = stdin;
    }
    else {
        fp = fopen(filename, "r");
        if (fp == NULL) {
            return NULL;
        }
    }
    int buffer_size = BUFSIZ;
    char *buffer = malloc(buffer_size);
    if (buffer == NULL) {
        fclose(fp);
        return NULL;
    }
    int read = 0;
    char *larger_buffer;
    do {
        read += fread(&buffer[read], 1, BUFSIZ, fp);
        if (ferror(fp) != 0) {
            free(buffer);
            fclose(fp);
            return NULL;
        }
        buffer_size += BUFSIZ;
        larger_buffer = realloc(buffer, buffer_size);
        if (larger_buffer == NULL) {
            free(buffer);
            fclose(fp);
            return NULL;
        }
        buffer = larger_buffer;
    } while (feof(fp) == 0);
    return buffer;
}

static bool is_whitespace(const char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int css_skip_whitespaces_comments(const char *css, int i, int *line, int *last_newline)
{
    do {
        while (is_whitespace(css[i])) {
            if (css[i] == '\n') {
                *line += 1;
                *last_newline = i;
            }
            i += 1;
        }
        if (css[i] == '\0' || css[i] != '/' || css[i + 1] != '*') {
            break;
        }
        i += 2;
        while (css[i] != '\0' && (css[i] != '*' || css[i + 1] != '/')) {
            if (css[i] == '\n') {
                *line += 1;
                *last_newline = i;
            }
            i += 1;
        }
        if (css[i] != '\0') {
            i += 2;
        }
    } while (true);
    return i;
}

static int strnicmp(const char *s1, const char *s2, unsigned int length)
{
    int diff = 0;
    while (length--) {
        if (diff = *s1 - *s2) {
            if ((unsigned char) (*s1 - 'A') <= 'Z' - 'A') {
                diff += 'a' - 'A';
            }
            if ((unsigned char) (*s2 - 'A') <= 'Z' - 'A') {
                diff -= 'a' - 'A';
            }
            if (diff != 0) {
                break;
            }
        }
        if (*s1 == '\0') {
            break;
        }
        s1++;
        s2++;
    }
    return diff;
}

static bool is_nestable_atrule(const char *atrule, unsigned int atrule_length)
{
    return
        sizeof "@media" - 1 == atrule_length &&
        ! strnicmp(atrule, "@media", atrule_length) ||

        sizeof "@layer " - 1 == atrule_length &&
        ! strnicmp(atrule, "@layer", atrule_length) ||

        sizeof "@container" - 1 == atrule_length &&
        ! strnicmp(atrule, "@container", atrule_length) ||

        sizeof "@keyframes" - 1 == atrule_length &&
        ! strnicmp(atrule, "@keyframes", atrule_length);
}

char *minify_css(const char *css)
{
    enum {
        SYNTAX_BLOCK_STYLE,
        SYNTAX_BLOCK_RULE_START,
        SYNTAX_BLOCK_QRULE,
        SYNTAX_BLOCK_QRULE_ROUND_BRACKETS,
        SYNTAX_BLOCK_QRULE_SQUARE_BRACKETS,
        SYNTAX_BLOCK_ATRULE,
        SYNTAX_BLOCK_ATRULE_ROUND_BRACKETS,
        SYNTAX_BLOCK_ATRULE_SQUARE_BRACKETS,
    } syntax_block = SYNTAX_BLOCK_RULE_START;

    char *css_min = malloc(strlen(css) + 1);
    if (css_min == NULL) {
        perror(NULL);
        return NULL;
    }
    int css_min_length = 0;

    const char *atrule = NULL;
    int atrule_length;

    int line = 1, last_newline = -1;
    int i = css_skip_whitespaces_comments(css, 0, &line, &last_newline);
    int nesting_level = 0;

    while (true) {
        if (css[i] == '\0') {
            if (syntax_block != SYNTAX_BLOCK_RULE_START) {
                if (syntax_block == SYNTAX_BLOCK_STYLE) {
                    fprintf(stderr, "Unexpected end of document, expected }\n");
                }
                else if (syntax_block == SYNTAX_BLOCK_QRULE) {
                    fprintf(stderr, "Unexpected end of document, expected {…}\n");
                }
                else if (syntax_block == SYNTAX_BLOCK_ATRULE) {
                    fprintf(stderr, "Unexpected end of document, expected ; or {…}\n");
                }
                free(css_min);
                return NULL;
            }
            css_min[css_min_length++] = '\0';
            break;
        }
        if (css[i - 1] == '\\') {
            css_min[css_min_length++] = css[i];
            i += 1;
            continue;
        }
        if (css[i] == '}') {
            do {
                if (nesting_level == 0) {
                    free(css_min);
                    fprintf(stderr, "Unexpected } in line %d, column %d\n", line, i - last_newline);
                    return NULL;
                }
                css_min[css_min_length++] = '}';
                nesting_level -= 1;
                i = css_skip_whitespaces_comments(css, i + 1, &line, &last_newline);
            } while (css[i] == '}');
            syntax_block = SYNTAX_BLOCK_RULE_START;
            continue;
        }
        if (syntax_block == SYNTAX_BLOCK_RULE_START) {
            if (css[i] == '{' || css[i] == '}' || css[i] == '"' || css[i] == '\'') {
                free(css_min);
                fprintf(stderr, "Unexpected %c in line %d, column %d\n",
                        css[i], line, i - last_newline);
                return NULL;
            }
            css_min[css_min_length++] = css[i];
            if (css[i] == '@') {
                syntax_block = SYNTAX_BLOCK_ATRULE;
                atrule = &css[i];
                i += 1;
                atrule_length = 1;
                while (isalnum(css[i])) {
                    css_min[css_min_length++] = css[i];
                    atrule_length += 1;
                    i += 1;
                }
            }
            else {
                syntax_block = SYNTAX_BLOCK_QRULE;
                i += 1;
            }
            continue;
        }
        if (i > 2 && css[i] == '(' && css[i - 1] == 'l' && css[i - 2] == 'r' && css[i - 3] == 'u') {
            i += 1;
            while (is_whitespace(css[i])) {
                i += 1;
            }
            css_min[css_min_length++] = '(';
            if (css[i] == '"' || css[i] == '\'') {
                char quot = css[i];
                do {
                    css_min[css_min_length++] = css[i];
                    i += 1;
                } while ((css[i] != quot || css[i - 1] == '\\') && css[i] != '\0');
                css_min[css_min_length++] = quot;
                i += 1;
                while (is_whitespace(css[i])) {
                    i += 1;
                }
                if (css[i] != ')') {
                    free(css_min);
                    fprintf(stderr, "Expected ) in line %d, column %d\n",
                            line, i - last_newline);
                    return NULL;
                }
            }
            else {
                while ((css[i] != ')' || css[i - 1] == '\\') && css[i] != '\0' &&
                    ! is_whitespace(css[i]))
                {
                    css_min[css_min_length++] = css[i];
                    i += 1;
                }
                while (is_whitespace(css[i])) {
                    i += 1;
                }
                if (css[i] != ')') {
                    free(css_min);
                    if (css[i] == '\0') {
                        fprintf(stderr, "Unexpected end of document, expected )\n");
                    }
                    else if (is_whitespace(css[i - 1])) {
                        fprintf(stderr, "Illegal white-space in URL in line %d, column %d\n",
                                line, i - last_newline - 1);
                    }
                    return NULL;
                }
            }
            css_min[css_min_length++] = ')';
            i += 1;
            continue;
        }
        if (css[i] == '"' || css[i] == '\'') {
            int k = i;
            css_min[css_min_length++] = css[i++];
            while ((css[i] != css[k] || css[i - 1] == '\\') && css[i] != '\0') {
                css_min[css_min_length++] = css[i];
                i += 1;
            }
            css_min[css_min_length++] = css[k];
            i += 1;
            continue;
        }
        if (css[i] == ';' && syntax_block != SYNTAX_BLOCK_QRULE) {
            do {
                i = css_skip_whitespaces_comments(css, i + 1, &line, &last_newline);
            } while (css[i] == ';');
            if (css[i] != '}') {
                css_min[css_min_length++] = ';';
            }
            if (syntax_block == SYNTAX_BLOCK_ATRULE) {
                syntax_block = SYNTAX_BLOCK_RULE_START;
            }
            continue;
        }
        if (css[i] == '{') {
            nesting_level += 1;
            if (syntax_block == SYNTAX_BLOCK_STYLE)  {
                free(css_min);
                fprintf(stderr, "Unexpected { in line %d, column %d\n", line, i - last_newline);
                return NULL;
            }
            css_min[css_min_length++] = '{';
            i = css_skip_whitespaces_comments(css, i + 1, &line, &last_newline);
            if (syntax_block == SYNTAX_BLOCK_QRULE) {
                syntax_block = SYNTAX_BLOCK_STYLE;
            }
            else if (syntax_block == SYNTAX_BLOCK_ATRULE) {
                if (is_nestable_atrule(atrule, atrule_length) == true) {
                    syntax_block = SYNTAX_BLOCK_RULE_START;
                }
                else {
                    syntax_block = SYNTAX_BLOCK_STYLE;
                }
            }
            continue;
        }
        if (css[i] == '0' && css[i + 1] == '.' && (i == 0 || css[i - 1] < '0' || css[i - 1] > '9')) {
            i += 1;
            continue;
        }
        if (css[i] == '(' && syntax_block == SYNTAX_BLOCK_ATRULE) {
            syntax_block = SYNTAX_BLOCK_ATRULE_ROUND_BRACKETS;
            css_min[css_min_length++] = '(';
            i += 1;
            continue;
        }
        if (css[i] == '[' && syntax_block == SYNTAX_BLOCK_ATRULE) {
            syntax_block = SYNTAX_BLOCK_ATRULE_SQUARE_BRACKETS;
            css_min[css_min_length++] = '[';
            i += 1;
            continue;
        }
        if (css[i] == ')' && syntax_block == SYNTAX_BLOCK_ATRULE_ROUND_BRACKETS) {
            syntax_block = SYNTAX_BLOCK_ATRULE;
            css_min[css_min_length++] = ')';
            i += 1;
            continue;
        }
        if (css[i] == ']' && syntax_block == SYNTAX_BLOCK_ATRULE_SQUARE_BRACKETS) {
            syntax_block = SYNTAX_BLOCK_ATRULE;
            css_min[css_min_length++] = ']';
            i += 1;
            continue;
        }
        if (css[i] == '(' && syntax_block == SYNTAX_BLOCK_QRULE) {
            syntax_block = SYNTAX_BLOCK_QRULE_ROUND_BRACKETS;
            css_min[css_min_length++] = '(';
            i += 1;
            continue;
        }
        if (css[i] == '[' && syntax_block == SYNTAX_BLOCK_QRULE) {
            syntax_block = SYNTAX_BLOCK_QRULE_SQUARE_BRACKETS;
            css_min[css_min_length++] = '[';
            i += 1;
            continue;
        }
        if (css[i] == ')' && syntax_block == SYNTAX_BLOCK_QRULE_ROUND_BRACKETS) {
            syntax_block = SYNTAX_BLOCK_QRULE;
            css_min[css_min_length++] = ')';
            i += 1;
            continue;
        }
        if (css[i] == ']' && syntax_block == SYNTAX_BLOCK_QRULE_SQUARE_BRACKETS) {
            syntax_block = SYNTAX_BLOCK_QRULE;
            css_min[css_min_length++] = ']';
            i += 1;
            continue;
        }
        if (is_whitespace(css[i]) || css[i] == '/' && css[i + 1] == '*') {
            if (syntax_block == SYNTAX_BLOCK_ATRULE_ROUND_BRACKETS ||
                syntax_block == SYNTAX_BLOCK_QRULE_ROUND_BRACKETS)
            {
                // We must remove white-space around ":" in "@media (with : 3 px){}"
                // but not in "@page :left{}".

                i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                if (
                    css_min[css_min_length - 1] != '(' &&
                    css_min[css_min_length - 1] != ',' &&
                    css_min[css_min_length - 1] != '<' &&
                    css_min[css_min_length - 1] != '>' &&
                    css_min[css_min_length - 1] != ':' &&
                    css[i] != ')' &&
                    css[i] != ',' &&
                    css[i] != '<' &&
                    css[i] != '>' &&
                    css[i] != ':'
                ) {
                    css_min[css_min_length++] = ' ';
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_ATRULE_SQUARE_BRACKETS ||
                     syntax_block == SYNTAX_BLOCK_QRULE_SQUARE_BRACKETS)
            {
                i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                if (
                    css_min[css_min_length - 1] != '[' &&
                    css_min[css_min_length - 1] != '=' &&
                    css_min[css_min_length - 1] != ',' &&
                    css[i] != ']' &&
                    css[i] != '=' &&
                    css[i] != ',' &&
                    css[i] != '*' &&
                    css[i] != '$' &&
                    css[i] != '^' &&
                    css[i] != '~' &&
                    css[i] != '|'
                ) {
                    css_min[css_min_length++] = ' ';
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_ATRULE) {
                int before_whitespace = i;
                i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                if (
                    // Remove white-space before ( in `@media (...){}` but not in
                    // `@media all and (...){}`.

                    (css[i] != '(' || &atrule[atrule_length - 1] != &css[before_whitespace] - 1) &&
                    css_min[css_min_length - 1] != ',' &&
                    css_min[css_min_length - 1] != ')' &&
                    css_min[css_min_length - 1] != '(' &&
                    css[i] != ',' &&
                    css[i] != ')' &&
                    css[i] != ';' &&
                    css[i] != '{'
                ) {
                    css_min[css_min_length++] = ' ';
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_QRULE) {
                i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                if (
                    css_min[css_min_length - 1] != '~' &&
                    css_min[css_min_length - 1] != '>' &&
                    css_min[css_min_length - 1] != '+' &&
                    css_min[css_min_length - 1] != ',' &&
                    css_min[css_min_length - 1] != ']' &&
                    css[i] != '~' &&
                    css[i] != '>' &&
                    css[i] != '+' &&
                    css[i] != ',' &&
                    css[i] != '[' &&
                    css[i] != '{'
                ) {
                    css_min[css_min_length++] = ' ';
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_STYLE) {
                i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                if (
                    css_min[css_min_length - 1] != '{' &&
                    css_min[css_min_length - 1] != ':' &&
                    css_min[css_min_length - 1] != ',' &&
                    css[i] != '}' &&
                    css[i] != ':' &&
                    css[i] != ',' &&
                    css[i] != ';' &&
                    css[i] != '!'
                ) {
                    css_min[css_min_length++] = ' ';
                }
            }
            continue;
        }
        css_min[css_min_length++] = css[i];
        i += 1;
    }
    char *css_min_realloc = realloc(css_min, css_min_length);
    if (css_min_realloc == NULL) {
        free(css_min);
        perror(NULL);
        return NULL;
    }
    return css_min_realloc;
}

static int js_skip_whitespaces_comments(const char *js, int i, int *line, int *last_newline)
{
    do {
        while (is_whitespace(js[i])) {
            if (js[i] == '\n') {
                *line += 1;
                *last_newline = i;
            }
            i += 1;
        }
        if (js[i] == '\0') {
            break;
        }
        else if (js[i] == '/' && js[i + 1] == '*') {
            i += 2;
            while (js[i] != '\0' && (js[i] != '*' || js[i + 1] != '/')) {
                if (js[i] == '\n') {
                    *line += 1;
                    *last_newline = i;
                }
                i += 1;
            }
            if (js[i] != '\0') {
                i += 2;
            }
        }
        else if (js[i] == '/' && js[i + 1] == '/') {
            i += 2;
            while (js[i] != '\0' && js[i] != '\n') {
                i += 1;
            }
        }
        else {
            break;
        }
    } while (true);
    return i;
}

char *minify_js(const char *js)
{
    char *js_min = malloc(strlen(js) + 1);
    if (js_min == NULL) {
        perror(NULL);
        return NULL;
    }
    int js_min_length = 0;
    int line = 1, last_newline = -1, i = 0;

    const char *trim_around_chars = "%<>+*/-=,(){}[]!~;|&^:?";
    const char *identifier_delimiters = "()[]{}; \t\r\n:=><+-*/~";

    enum {
        CURLY_BLOCK_UNKNOWN,
        CURLY_BLOCK_FUNCBODY,
    } curly_block[256];
    int curly_nesting_level = 0;

    enum {
        ROUND_BLOCK_UNKNOWN,
        ROUND_BLOCK_FUNCDEF_PARAMS,
    } round_block[256];
    int round_nesting_level = 0;

    while (true) {
        if (js[i] == '\0') {
            js_min[js_min_length++] = '\0';
            break;
        }
        if (
            ! strncmp(&js[i], "function", sizeof "function" - 1) &&
            strchr(identifier_delimiters, js[i + sizeof "function" - 1]) != NULL
        ) {
            // Regular functions cannot be safely replaced by arrow functions.  Arrow functions
            // cannot be used as constructors: `new arrow_function()` where `arrow_function` is an
            // arrow function is invalid.

            // Here we consume the input until ( of the parameter list.

            strcpy(&js_min[js_min_length], "function");
            js_min_length += sizeof "function" - 1;

            i += sizeof "function" - 1;
            i = js_skip_whitespaces_comments(js, i, &line, &last_newline);
            if (js[i] != '(') {
                js_min[js_min_length++] = ' ';
                while (strchr(identifier_delimiters, js[i]) == NULL) {
                    js_min[js_min_length++] = js[i++];
                }
            }
            i = js_skip_whitespaces_comments(js, i, &line, &last_newline);
            if (js[i] != '(') {
                fprintf(stderr, "Expected ( in line %d, column %d\n", line, i - last_newline);
                free(js_min);
                return NULL;
            }
            if (++round_nesting_level == sizeof round_block / sizeof round_block[0]) {
                free(js_min);
                fprintf(stderr, "The nesting level of round brackets is too deep");
                return NULL;
            }
            round_block[round_nesting_level] = ROUND_BLOCK_FUNCDEF_PARAMS;
            js_min[js_min_length++] = '(';
            i += 1;
            continue;
        }
        if (js[i] == '{') {
            if (++curly_nesting_level == sizeof curly_block / sizeof curly_block[0]) {
                free(js_min);
                fprintf(stderr, "The nesting level of curly brackets is too deep");
                return NULL;
            }
            if (
                js_min_length > 2 && (
                    js_min[js_min_length - 2] == '=' && js_min[js_min_length - 1] == '>' ||
                    js_min[js_min_length - 1] == ')' &&
                    round_block[round_nesting_level + 1] == ROUND_BLOCK_FUNCDEF_PARAMS
                )
            ) {
                curly_block[curly_nesting_level] = CURLY_BLOCK_FUNCBODY;
            }
            else {
                curly_block[curly_nesting_level] = CURLY_BLOCK_UNKNOWN;
            }
            js_min[js_min_length++] = '{';
            i += 1;
            continue;
        }
        if (js[i] == '(') {
            if (++round_nesting_level == sizeof round_block / sizeof round_block[0]) {
                free(js_min);
                fprintf(stderr, "The nesting level of round brackets is too deep");
                return NULL;
            }

            i += 1;

            // Removing round brackets around single parameters with no default
            // value of arrow functions.

            bool remove_round_brackets_around_param = false;

            int k = i;
            k = js_skip_whitespaces_comments(js, k, &line, &last_newline);
            int arg_start = k;
            do {
                k += 1;
            } while (strchr(",)= \n\r\t/*", js[k]) == NULL);
            int arg_end = k;
            k = js_skip_whitespaces_comments(js, k, &line, &last_newline);
            if (js[k] == ')') {
                k = js_skip_whitespaces_comments(js, k + 1, &line, &last_newline);
                if (js[k] == '=' && js[k + 1] == '>') {
                    remove_round_brackets_around_param = true;
                }
            }

            if (remove_round_brackets_around_param) {
                strncpy(&js_min[js_min_length], &js[arg_start], arg_end - arg_start);
                js_min_length += arg_end - arg_start;
                i = k;
            }
            else {
                round_block[round_nesting_level] = ROUND_BLOCK_UNKNOWN;
                js_min[js_min_length++] = '(';
            }
            continue;
        }
        if (js[i] == '}') {
            if (--curly_nesting_level < 0) {
                free(js_min);
                fprintf(stderr, "Unexpected } in line %d, column %d\n", line, i - last_newline);
                return NULL;
            }
            js_min[js_min_length++] = '}';
            i += 1;
            continue;
        }
        if (js[i] == ')') {
            if (--round_nesting_level < 0) {
                free(js_min);
                fprintf(stderr, "Unexpected ) in line %d, column %d\n", line, i - last_newline);
                return NULL;
            }
            js_min[js_min_length++] = ')';
            i += 1;
            continue;
        }
        if (js[i] == ';') {
            char before_semicolon = (js_min_length == 0 ? '}' : js_min[js_min_length - 1]);
            do {
                i = js_skip_whitespaces_comments(js, i + 1, &line, &last_newline);
            } while (js[i] == ';');

            // `;` can be removed after `}`, except if it ends a function body.
            // Semicolon cannot be removed in `a=()=>{};b=0`.

            if (
                (before_semicolon != '}' ||
                curly_block[curly_nesting_level + 1] == CURLY_BLOCK_FUNCBODY) &&
                js[i] != '}' && js[i] != '\0'
            ) {
                js_min[js_min_length++] = ';';
            }
            continue;
        }
        if (js[i] == '/' && js[i + 1] != '/' && js[i + 1] != '*' &&
            (js_min_length == 0 || strchr("^!&|([><+-*i%", js_min[js_min_length - 1]) != NULL))
        {
            // This is a regex object.

            js_min[js_min_length++] = '/';
            i += 1;
            while (js[i] != '\0' && (js[i] != '/' || js[i - 1] == '\\')) {
                js_min[js_min_length++] = js[i];
                i += 1;
            }
            js_min[js_min_length++] = '/';
            i += 1;
            continue;
        }
        if (js[i] == '`' || js[i] == '"' || js[i] == '\'') {
            char quot = js[i];
            i += 1;
            js_min[js_min_length++] = quot;
            while (js[i] != '\0' && (js[i] != quot || js[i - 1] == '\\')) {
                js_min[js_min_length++] = js[i];
                i += 1;
            }
            if (js[i] != quot) {
                free(js_min);
                fprintf(stderr, "Unexpected end of document, expected %c\n", quot);
                return NULL;
            }
            js_min[js_min_length++] = quot;
            i += 1;
            continue;
        }
        if ((i == 0 || strchr(identifier_delimiters, js[i - 1]) != NULL) &&
            ! strncmp(&js[i], "true", sizeof "true" - 1) &&
            strchr(identifier_delimiters, js[i + sizeof "true" - 1]) != NULL)
        {
            js_min[js_min_length++] = '!';
            js_min[js_min_length++] = '0';
            i += sizeof "true" - 1;
            continue;
        }
        if ((i == 0 || strchr(identifier_delimiters, js[i - 1]) != NULL) &&
            ! strncmp(&js[i], "false", sizeof "false" - 1) &&
            strchr(identifier_delimiters, js[i + sizeof "false" - 1]) != NULL)
        {
            js_min[js_min_length++] = '!';
            js_min[js_min_length++] = '1';
            i += sizeof "false" - 1;
            continue;
        }
        if (is_whitespace(js[i]) ||
            js[i] == '/' && js[i + 1] == '*' ||
            js[i] == '/' && js[i + 1] == '/')
        {
            int old_line = line;
            i = js_skip_whitespaces_comments(js, i, &line, &last_newline);
            if (old_line != line && js_min[js_min_length - 1] == '}' &&
                curly_block[curly_nesting_level + 1] == CURLY_BLOCK_FUNCBODY &&
                js[i] != '\0' && js[i] != '}')
            {
                js_min[js_min_length++] = ';';
                continue;
            }
            if (i == 0 || strchr(trim_around_chars, js_min[js_min_length - 1]) != NULL) {
                continue;
            }
            if (strchr(trim_around_chars, js[i]) == NULL) {
                js_min[js_min_length++] = (old_line != line ? '\n' : ' ');
            }
            continue;
        }
        js_min[js_min_length++] = js[i];
        i += 1;
    }
    char *js_min_realloc = realloc(js_min, js_min_length);
    if (js_min_realloc == NULL) {
        free(js_min);
        perror(NULL);
        return NULL;
    }
    return js_min;
}

enum sgml_subset {
    SGML_SUBSET_XML,
    SGML_SUBSET_HTML,
};

static char *minify_sgml(const char *sgml, enum sgml_subset sgml_subset)
{
    int line = 1, last_newline = -1, i = 0;
    while (is_whitespace(sgml[i])) {
        if (sgml[i] == '\n') {
            line += 1;
            last_newline = i;
        }
        i += 1;
    }
    if (sgml[i] != '<' && sgml[i] != '\0') {
        fprintf(stdout, "Expected < in line %d, column %d\n", line, i - last_newline);
        return NULL;
    }

    char *sgml_min = malloc(strlen(sgml) + 1);
    if (sgml_min == NULL) {
        perror(NULL);
        return NULL;
    }
    int sgml_min_length = 0;

    enum {
        SYNTAX_BLOCK_TAG,
        SYNTAX_BLOCK_CONTENT,
        SYNTAX_BLOCK_DOCTYPE,
    } syntax_block = SYNTAX_BLOCK_CONTENT;

    const char *current_tag;
    int current_tag_length;
    bool is_closing_tag;
    bool whitespace_before_tag;

    while (true) {
        if (sgml[i] == '\0') {
            if (syntax_block == SYNTAX_BLOCK_TAG) {
                free(sgml_min);
                fprintf(stderr, "Unexpected end of document, expected >\n",
                        line, i - last_newline);
                return NULL;
            }
            sgml_min[sgml_min_length++] = '\0';
            break;
        }
        if (! strncmp(&sgml[i], "<!--", 4)) {
            i += 4;
            while (sgml[i] != '\0' && strncmp(&sgml[i], "-->", 3)) {
                if (sgml[i] == '\n') {
                    line += 1;
                    last_newline = i;
                }
                i += 1;
            }
            if (sgml[i] == '\0') {
                free(sgml_min);
                fprintf(stderr, "Unexpected end of document inside a comment\n",
                        line, i - last_newline);
                return NULL;
            }
            i += 3;

            // Trim whitespace at the end of the document

            int k = i;
            while (is_whitespace(sgml[k])) {
                k += 1;
            }
            if (sgml[k] == '\0') {
                i = k;
            }
            continue;
        }
        if (sgml[i] == '<') {
            if (syntax_block == SYNTAX_BLOCK_TAG) {
                free(sgml_min);
                fprintf(stderr, "Invalid < character in line %d, column %d\n",
                        line, i - last_newline);
                return NULL;
            }
            if (is_whitespace(sgml[i + 1])) {
                free(sgml_min);
                fprintf(stderr, "Invalid whitespace after < in line %d, column %d\n",
                        line, i - last_newline);
                return NULL;
            }
            if (sgml[i + 1] == '/' && is_whitespace(sgml[i + 1])) {
                free(sgml_min);
                fprintf(stderr, "Invalid whitespace after / in line %d, column %d\n",
                        line, i - last_newline + 1);
                return NULL;
            }
            whitespace_before_tag =
                sgml_min_length > 0 && is_whitespace(sgml_min[sgml_min_length - 1]);
            i += 1;
            if (! strnicmp(&sgml[i], "!DOCTYPE", sizeof "!DOCTYPE" - 1)) {
                syntax_block = SYNTAX_BLOCK_DOCTYPE;
                sgml_min[sgml_min_length++] = '<';
                continue;
            }

            is_closing_tag = (sgml[i] == '/');
            current_tag = &sgml[i];
            current_tag_length = 0;
            while (sgml[i + current_tag_length] != '>' &&
                   sgml[i + current_tag_length] != '\0' &&
                   ! is_whitespace(sgml[i + current_tag_length]))
            {
                current_tag_length += 1;
            }
            syntax_block = SYNTAX_BLOCK_TAG;
            sgml_min[sgml_min_length++] = '<';
            continue;
        }
        if (sgml[i] == '>') {
            if (syntax_block == SYNTAX_BLOCK_CONTENT) {
                free(sgml_min);
                fprintf(stderr, "Invalid > character in line %d, column %d\n",
                        line, i - last_newline);
                return NULL;
            }
            if (sgml[i - 1] == '/') {
                is_closing_tag = true;
            }

            // Transform `<sgml></sgml>` to `<sgml/>`. This would be illegal for HTML.

            else if (sgml_subset == SGML_SUBSET_XML &&
                     syntax_block != SYNTAX_BLOCK_DOCTYPE &&
                     sgml[i + 1] == '<' && sgml[i + 2] == '/' &&
                     ! strncmp(current_tag, &sgml[i + 3], current_tag_length) &&
                     (is_whitespace(sgml[i + 3 + current_tag_length]) ||
                     sgml[i + 3 + current_tag_length] == '>'))
            {
                sgml_min[sgml_min_length++] = '/';
                sgml_min[sgml_min_length++] = '>';
                i += 3 + current_tag_length;
                while (sgml[i] != '>' && sgml[i] != '\0') {
                    i += 1;
                }
                syntax_block = SYNTAX_BLOCK_CONTENT;
                i += 1;
                continue;
            }

            i += 1;
            syntax_block = SYNTAX_BLOCK_CONTENT;

            if (sgml_subset == SGML_SUBSET_XML) {
                // Ignore whitespace except between opening and closing tags

                int k = i;
                int k_line = line;
                int k_last_newline = last_newline;
                while (is_whitespace(sgml[k])) {
                    if (sgml[k] == '\n') {
                        k_line += 1;
                        k_last_newline = k;
                    }
                    k += 1;
                }
                if (sgml[k] == '<' && (is_closing_tag || sgml[k + 1] != '/')) {
                    line = k_line;
                    last_newline = k_last_newline;
                    i = k;
                }
            }

            sgml_min[sgml_min_length++] = '>';

            // Trim whitespace at the end of the document

            int k = i;
            while (is_whitespace(sgml[k])) {
                k += 1;
            }
            if (sgml[k] == '\0') {
                i = k;
            }
            continue;
        }
        if (sgml_subset == SGML_SUBSET_HTML && sgml[i] == '/' && sgml[i + 1] == '>') {
            i += 1;
            continue;
        }
        if (sgml[i] == '"' || sgml[i] == '\'') {
            if (syntax_block != SYNTAX_BLOCK_DOCTYPE &&
               (sgml_min_length == 0 || sgml_min[sgml_min_length - 1] != '='))
            {
                free(sgml_min);
                fprintf(stderr, "Expected = before quote in line %d, column %d\n",
                        line, i - last_newline);
                return NULL;
            }
            char quote = sgml[i];
            i += 1;
            bool need_quotes;
            if (sgml_subset == SGML_SUBSET_XML || syntax_block == SYNTAX_BLOCK_DOCTYPE) {
                need_quotes = true;
            }
            else {
                need_quotes = false;
                int k = i;
                while (sgml[k] != quote) {
                    if (is_whitespace(sgml[k])) {
                        need_quotes = true;
                        break;
                    }
                    k += 1;
                }
            }
            if (need_quotes) {
                sgml_min[sgml_min_length++] = quote;
            }
            while (sgml[i] != '\0' && sgml[i] != quote) {
                sgml_min[sgml_min_length++] = sgml[i];
                i += 1;
            }
            if (sgml[i] == '\0') {
                free(sgml_min);
                fprintf(stderr, "Unexpected end of document, expected %c\n", quote);
                return NULL;
            }
            if (need_quotes) {
                sgml_min[sgml_min_length++] = quote;
            }
            // This we can omit when we remove / before >:
            // else if (sgml[i + 1] == '/') {
            //    sgml_min[sgml_min_length++] = ' ';
            // }
            i += 1;
            continue;
        }
        if (syntax_block == SYNTAX_BLOCK_TAG && is_whitespace(sgml[i])) {
            while (is_whitespace(sgml[i])) {
                if (sgml[i] == '\n') {
                    line += 1;
                    last_newline = i;
                }
                i += 1;
            }
            if (sgml[i] != '=' && sgml_min[sgml_min_length - 1] != '=' && sgml[i] != '>' &&
                sgml[i] != '/')
            {
                sgml_min[sgml_min_length++] = ' ';
            }
            continue;
        }
        if (sgml_subset == SGML_SUBSET_HTML && syntax_block == SYNTAX_BLOCK_CONTENT &&
            current_tag_length == sizeof "script" - 1 &&
            ! strnicmp(current_tag, "script", sizeof "script" - 1))
        {
            // The difference between `script` and `pre` tags is that XML comments
            // must not be removed from the content of script tags.

            while (sgml[i] != '\0' && strncmp(&sgml[i], "</script", sizeof "</script" - 1)) {
                sgml_min[sgml_min_length++] = sgml[i];
                i += 1;
            }
            continue;
        }
        if (sgml_subset == SGML_SUBSET_HTML &&
            syntax_block == SYNTAX_BLOCK_CONTENT && is_whitespace(sgml[i]))
        {
            if (current_tag_length == sizeof "pre" - 1 &&
                ! strnicmp(current_tag, "pre", sizeof "pre" - 1))
            {
                if (sgml[i] == '\n') {
                    line += 1;
                    last_newline = i;
                }
                sgml_min[sgml_min_length++] = sgml[i];
                i += 1;
                continue;
            }
            do {
                while (is_whitespace(sgml[i])) {
                    if (sgml[i] == '\n') {
                        line += 1;
                        last_newline = i;
                    }
                    i += 1;
                }
                if (sgml[i] == '\0' || strncmp(&sgml[i], "<!--", 4)) {
                    break;
                }
                i += 4;
                while (sgml[i] != '\0' && strncmp(&sgml[i], "-->", 3)) {
                    if (sgml[i] == '\n') {
                        line += 1;
                        last_newline = i;
                    }
                    i += 1;
                }
                if (sgml[i] == '\0') {
                    free(sgml_min);
                    fprintf(stderr, "Unexpected end of document inside a comment\n");
                    return NULL;
                }
                i += 3;
            } while (true);
            if (! whitespace_before_tag && sgml[i] != '\0') {
                sgml_min[sgml_min_length++] = ' ';
            }
            continue;
        }
        sgml_min[sgml_min_length++] = sgml[i];
        i += 1;
    }
    char *sgml_min_realloc = realloc(sgml_min, sgml_min_length);
    if (sgml_min_realloc == NULL) {
        free(sgml_min);
        perror(NULL);
        return NULL;
    }
    return sgml_min;
}

char *minify_xml(const char *xml)
{
    return minify_sgml(xml, SGML_SUBSET_XML);
}

char *minify_html(const char *html)
{
    return minify_sgml(html, SGML_SUBSET_HTML);
}

void print_usage()
{
    fputs("Usage: minify <css|js|xml|html> <input file|-> [--benchmark]\n", stderr);
}

int main(int argc, char *argv[])
{
    bool benchmark = false;
    const char *format_str = NULL;
    const char *input_filename = NULL;
    for (int i = 1; i < argc; ++i) {
        if (! strcmp(argv[i], "--benchmark")) {
            benchmark = true;
        }
        else if (format_str == NULL) {
            format_str = argv[i];
        }
        else if (input_filename == NULL) {
            input_filename = argv[i];
        }
        else {
            print_usage();
            return EXIT_FAILURE;
        }
    }
    if (format_str == NULL || input_filename == NULL) {
        print_usage();
        return EXIT_FAILURE;
    }
    enum {
        FORMAT_JS,
        FORMAT_CSS,
        FORMAT_XML,
        FORMAT_HTML,
    } format;
    if (! strcmp(format_str, "js")) {
        format = FORMAT_JS;
    }
    else if (! strcmp(format_str, "css")) {
        format = FORMAT_CSS;
    }
    else if (! strcmp(format_str, "xml")) {
        format = FORMAT_XML;
    }
    else if (! strcmp(format_str, "html")) {
        format = FORMAT_HTML;
    }
    else {
        fprintf(stderr, "Unsupported input format: %s\n", format_str);
        print_usage();
        return EXIT_FAILURE;
    }

    char *input = file_get_content(input_filename);
    if (input == NULL) {
        perror(input_filename);
        return EXIT_FAILURE;
    }
    char *input_min;
    switch (format) {
    case FORMAT_JS:
        input_min = minify_js(input);
        break;
    case FORMAT_CSS:
        input_min = minify_css(input);
        break;
    case FORMAT_XML:
        input_min = minify_xml(input);
        break;
    case FORMAT_HTML:
        input_min = minify_html(input);
        break;
    default:
        input_min = NULL;
    }
    if (input_min == NULL) {
        return EXIT_FAILURE;
    }
    if (benchmark) {
        int strlen_input = strlen(input);
        int strlen_input_min = strlen(input_min);
        printf(
            "Reduced the size from %d to %d bytes (%.1f%% of the original size)\n",
            strlen_input, strlen_input_min, 100.0 * strlen_input_min / strlen_input
        );
    }
    else {
        fputs(input_min, stdout);
    }
    free(input);
    free(input_min);
    return EXIT_SUCCESS;
}

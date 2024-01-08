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

static bool skip_whitespaces_comments(const char *js, int *i, char *js_min, int *js_min_length,
    int *line, int *last_newline, bool support_double_slash_comments)
{
    bool preserve_comment = false;
    int preserved_comment_start;
    do {
        while (is_whitespace(js[*i])) {
            if (line != NULL && js[*i] == '\n') {
                *line += 1;
                *last_newline = *i;
            }
            *i += 1;
        }
        preserved_comment_start = -1;
        if (js[*i] == '\0') {
            break;
        }
        else if (js[*i] == '/' && js[*i + 1] == '*') {
            if (js[*i + 2] == '!') {
                preserved_comment_start = *i;
            }
            *i += 2;
            while (js[*i] != '\0' && (js[*i] != '*' || js[*i + 1] != '/')) {
                if (line != NULL && js[*i] == '\n') {
                    *line += 1;
                    *last_newline = *i;
                }
                *i += 1;
            }
            if (js[*i] != '\0') {
                *i += 2;
            }
        }
        else if (support_double_slash_comments && js[*i] == '/' && js[*i + 1] == '/') {
            *i += 2;
            while (js[*i] != '\0' && js[*i] != '\n') {
                *i += 1;
            }
        }
        else {
            break;
        }
        if (preserved_comment_start >= 0) {
            preserve_comment = true;
            if (js_min != NULL) {
                strncpy(&js_min[*js_min_length], &js[preserved_comment_start], *i - preserved_comment_start);
                *js_min_length += *i - preserved_comment_start;
            }
        }
    } while (true);
    return preserve_comment;
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
        !strnicmp(atrule, "@media", atrule_length) ||

        sizeof "@layer " - 1 == atrule_length &&
        !strnicmp(atrule, "@layer", atrule_length) ||

        sizeof "@container" - 1 == atrule_length &&
        !strnicmp(atrule, "@container", atrule_length) ||

        sizeof "@keyframes" - 1 == atrule_length &&
        !strnicmp(atrule, "@keyframes", atrule_length);
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
    int i = 0;
    skip_whitespaces_comments(css, &i, css_min, &css_min_length, &line, &last_newline, false);
    int nesting_level = 0;

    while (true) {
        if (css[i] == '\0') {
            if (syntax_block != SYNTAX_BLOCK_RULE_START) {
                free(css_min);
                if (syntax_block == SYNTAX_BLOCK_STYLE) {
                    fprintf(stderr, "Unexpected end of document, expected }\n");
                }
                else if (syntax_block == SYNTAX_BLOCK_QRULE) {
                    fprintf(stderr, "Unexpected end of document, expected {…}\n");
                }
                else if (syntax_block == SYNTAX_BLOCK_ATRULE) {
                    fprintf(stderr, "Unexpected end of document, expected ; or {…}\n");
                }
                else {
                    fprintf(stderr, "Unexpected end of document\n");
                }
                return NULL;
            }
            css_min[css_min_length++] = '\0';
            break;
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
                i += 1;
                skip_whitespaces_comments(css, &i, css_min, &css_min_length, &line, &last_newline, false);
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
                // TODO: active_backslash
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
                    !is_whitespace(css[i]))
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
        if (css[i] == '\\') {
            css_min[css_min_length++] = css[i++];
            bool active_backslash = true;
            while (css[i] == '\\') {
                active_backslash = !active_backslash;
                css_min[css_min_length++] = css[i++];
            }
            if (active_backslash) {
                css_min[css_min_length++] = css[i++];
            }
            continue;
        }
        if (css[i] == '"' || css[i] == '\'') {
            int k = i;
            int column_start = i - last_newline;
            css_min[css_min_length++] = css[i++];
            bool active_backslash = false;
            while (css[i] != '\0' && (css[i] != css[k] || active_backslash)) {
                if (css[i] == '\\') {
                    active_backslash = !active_backslash;
                }
                else {
                    active_backslash = false;
                }
                css_min[css_min_length++] = css[i];
                i += 1;
            }
            if (css[i] == '\0') {
                free(css_min);
                fprintf(stderr, "Unclosed string starting in line %d, column %d\n", line, column_start);
                return NULL;
            }
            css_min[css_min_length++] = css[k];
            i += 1;
            continue;
        }
        if (css[i] == ';' && syntax_block != SYNTAX_BLOCK_QRULE) {
            do {
                i += 1;
                skip_whitespaces_comments(css, &i, css_min, &css_min_length, &line, &last_newline, false);
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
            i += 1;
            skip_whitespaces_comments(css, &i, css_min, &css_min_length, &line, &last_newline, false);
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
            // Converting for example `0.1` to `.1`
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

                skip_whitespaces_comments(css, &i, css_min, &css_min_length, &line, &last_newline, false);
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
                skip_whitespaces_comments(css, &i, css_min, &css_min_length, &line, &last_newline, false);
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
                skip_whitespaces_comments(css, &i, css_min, &css_min_length, &line, &last_newline, false);
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
                skip_whitespaces_comments(css, &i, css_min, &css_min_length, &line, &last_newline, false);
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
                skip_whitespaces_comments(css, &i, css_min, &css_min_length, &line, &last_newline, false);
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

char *minify_js(const char *js)
{
    char *js_min = malloc(strlen(js) + 1);
    if (js_min == NULL) {
        perror(NULL);
        return NULL;
    }
    int js_min_length = 0;
    int line = 1, last_newline = -1, i = 0;

    const char *identifier_delimiters = "'\"`%<>+*/-=,(){}[]!~;|&^:? \t\r\n";

    enum {
        CURLY_BLOCK_UNKNOWN,
        CURLY_BLOCK_FUNC_BODY,
        CURLY_BLOCK_CONDITION_BODY,
        CURLY_BLOCK_ARROWFUNC_BODY,
    } curly_block[64];
    int curly_nesting_level = 0;

    enum {
        ROUND_BLOCK_UNKNOWN,
        ROUND_BLOCK_CONDITION,
        ROUND_BLOCK_PARAM,
        ROUND_BLOCK_PARAM_ARROWFUNC_SINGLE,
    } round_block[64];
    int round_nesting_level = 0;

    while (true) {
        if (js[i] == '\0') {
            js_min[js_min_length++] = '\0';
            break;
        }

        // We determine the next keyword or identifier

        char next_identifier[1024];
        int next_identifier_length = 0;

        while (strchr(identifier_delimiters, js[i]) == NULL) {
            if (next_identifier_length >= sizeof next_identifier - 1) {
                fprintf(stderr, "Too long identifier in line %d, column %d\n", line,
                    i - next_identifier_length - last_newline);
                free(js_min);
                return NULL;
            }
            next_identifier[next_identifier_length++] = js[i++];
        }
        next_identifier[next_identifier_length] = '\0';

        /*
        next_identifier_length = strcspn(&js[i], identifier_delimiters);
        if (next_identifier_length > 0) {
            memcpy(next_identifier, &js[i], next_identifier_length);
            i += next_identifier_length;
        }
        next_identifier[next_identifier_length] = '\0';
        */

        // Keywords lose their meaning when used as object keys

        if (next_identifier_length > 0) {
            int k = i;
            skip_whitespaces_comments(js, &k, NULL, NULL, NULL, NULL, true);
            if (js[k] == ':') {
                strcpy(&js_min[js_min_length], next_identifier);
                js_min_length += next_identifier_length;
                continue;
            }
        }

        // Next we handle keywords

        if (!strcmp(next_identifier, "function")) {
            // We consume the input until `(` of the parameter list.
            //
            // Regular functions cannot be safely replaced by arrow functions.  Arrow functions
            // cannot be used as constructors: `new arrow_function()` where `arrow_function` is an
            // arrow function is invalid.

            strcpy(&js_min[js_min_length], next_identifier);
            js_min_length += next_identifier_length;

            skip_whitespaces_comments(js, &i, js_min, &js_min_length, &line, &last_newline, true);

            if (js[i] == '*') {
                js_min[js_min_length++] = '*';
                i += 1;
                skip_whitespaces_comments(js, &i, js_min, &js_min_length, &line, &last_newline, true);
            }
            if (js[i] != '(') {
                js_min[js_min_length++] = ' ';
                while (strchr(identifier_delimiters, js[i]) == NULL) {
                    js_min[js_min_length++] = js[i++];
                }
            }
            skip_whitespaces_comments(js, &i, js_min, &js_min_length, &line, &last_newline, true);
            if (js[i] != '(') {
                fprintf(stderr, "Expected `(` in line %d, column %d\n", line, i - last_newline);
                free(js_min);
                return NULL;
            }
            if (++round_nesting_level == sizeof round_block / sizeof round_block[0]) {
                free(js_min);
                fprintf(stderr, "The nesting level of round brackets is too deep\n");
                return NULL;
            }
            round_block[round_nesting_level] = ROUND_BLOCK_PARAM;
            js_min[js_min_length++] = '(';
            i += 1;
            continue;
        }
        if (
            !strcmp(next_identifier, "if") ||
            !strcmp(next_identifier, "for") ||
            !strcmp(next_identifier, "while")
        ) {
            strcpy(&js_min[js_min_length], next_identifier);
            js_min_length += next_identifier_length;
            skip_whitespaces_comments(js, &i, js_min, &js_min_length, &line, &last_newline, true);
            if (js[i] != '(') {
                free(js_min);
                fprintf(stderr, "Expected `(` in line %d, column %d\n", line, i - last_newline);
                return NULL;
            }
            if (++round_nesting_level == sizeof round_block / sizeof round_block[0]) {
                free(js_min);
                fprintf(stderr, "The nesting level of round brackets is too deep\n");
                return NULL;
            }
            i += 1;
            round_block[round_nesting_level] = ROUND_BLOCK_CONDITION;
            js_min[js_min_length++] = '(';
            continue;
        }
        if (!strcmp(next_identifier, "else")) {
            strcpy(&js_min[js_min_length], next_identifier);
            js_min_length += next_identifier_length;
            int k = i;
            skip_whitespaces_comments(js, &k, NULL, NULL, NULL, NULL, true);
            if (js[k] != '{') {
                continue;
            }
            skip_whitespaces_comments(js, &i, js_min, &js_min_length, &line, &last_newline, true);
            i += 1;
            k = i;
            bool preserved_comment =
                skip_whitespaces_comments(js, &k, js_min, &js_min_length, &line, &last_newline, true);
            if (!preserved_comment && js[k] == '}') {
                skip_whitespaces_comments(js, &i, js_min, &js_min_length, &line, &last_newline, true);
                js_min[js_min_length++] = ';';
                do {
                    i += 1;
                    skip_whitespaces_comments(js, &i, js_min, &js_min_length, &line, &last_newline, true);
                } while (js[i] == ';');
            }
            else {
                if (++curly_nesting_level == sizeof curly_block / sizeof curly_block[0]) {
                    free(js_min);
                    fprintf(stderr, "The nesting level of curly brackets is too deep\n");
                    return NULL;
                }
                js_min[js_min_length++] = '{';
                curly_block[curly_nesting_level] = CURLY_BLOCK_CONDITION_BODY;
            }
            continue;
        }
        if (!strcmp(next_identifier, "undefined")) {
            strcpy(&js_min[js_min_length], "void 0");
            js_min_length += sizeof "void 0" - 1;
            continue;
        }
        if (!strcmp(next_identifier, "true") || !strcmp(next_identifier, "false")) {
            if (js_min_length > 0 && js_min[js_min_length - 1] == ' ') {
                js_min_length -= 1;
            }
            js_min[js_min_length++] = '!';
            js_min[js_min_length++] = next_identifier[0] == 't' ? '0' : '1';
            continue;
        }

        if (next_identifier[0] != '\0') {
            strcpy(&js_min[js_min_length], next_identifier);
            js_min_length += next_identifier_length;
        }

        // Below we handle special characters

        if (js[i] == '{') {
            if (++curly_nesting_level == sizeof curly_block / sizeof curly_block[0]) {
                free(js_min);
                fprintf(stderr, "The nesting level of curly brackets is too deep\n");
                return NULL;
            }
            i += 1;
            if (js_min_length > 0 && js_min[js_min_length - 1] == ')' &&
                round_block[round_nesting_level + 1] == ROUND_BLOCK_CONDITION)
            {
                // Replacing `if(1){}` by `if(1);`
                bool preserved_comment =
                    skip_whitespaces_comments(js, &i, js_min, &js_min_length, &line, &last_newline, true);
                if (!preserved_comment && js[i] == '}') {
                    js_min[js_min_length++] = ';';
                    do {
                        i += 1;
                        skip_whitespaces_comments(js, &i, js_min, &js_min_length, &line, &last_newline, true);
                    } while (js[i] == ';');
                    curly_nesting_level -= 1;
                    continue;
                }
                else {
                    curly_block[curly_nesting_level] = CURLY_BLOCK_CONDITION_BODY;
                }
            }
            else if (js_min_length >= 2 &&
                     js_min[js_min_length - 2] == '=' && js_min[js_min_length - 1] == '>')
            {
                curly_block[curly_nesting_level] = CURLY_BLOCK_ARROWFUNC_BODY;
            }
            else if (js_min_length >= 1 && js_min[js_min_length - 1] == ')' &&
                     round_block[round_nesting_level + 1] == ROUND_BLOCK_PARAM)
            {
                curly_block[curly_nesting_level] = CURLY_BLOCK_FUNC_BODY;
            }
            else {
                curly_block[curly_nesting_level] = CURLY_BLOCK_UNKNOWN;
            }
            js_min[js_min_length++] = '{';
            continue;
        }
        if (js[i] == '(') {
            if (++round_nesting_level == sizeof round_block / sizeof round_block[0]) {
                free(js_min);
                fprintf(stderr, "The nesting level of round brackets is too deep\n");
                return NULL;
            }

            i += 1;

            // Removing round brackets around single parameters with no default
            // value of arrow functions.

            bool remove_round_brackets_around_param = false;

            int k = i;
            skip_whitespaces_comments(js, &k, NULL, NULL, NULL, NULL, true);
            if (js[k] != '.') { // Can't remove round brackets in `(...arg)=>{}`
                int arg_start = k;
                while (strchr(identifier_delimiters, js[k]) == NULL) {
                    k += 1;
                }
                int arg_end = k;
                skip_whitespaces_comments(js, &k, NULL, NULL, NULL, NULL, true);
                if (k > arg_start && js[k] == ')') {
                    k += 1;
                    skip_whitespaces_comments(js, &k, NULL, NULL, NULL, NULL, true);
                    if (js[k] == '=' && js[k + 1] == '>') {
                        remove_round_brackets_around_param = true;
                    }
                }
            }

            if (remove_round_brackets_around_param) {
                round_block[round_nesting_level] = ROUND_BLOCK_PARAM_ARROWFUNC_SINGLE;
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
                printf("%.*s\n", 100, &js[i]);
                fprintf(stderr, "Unexpected } in line %d, column %d\n", line, i - last_newline);
                return NULL;
            }
            js_min[js_min_length++] = '}';
            i += 1;
            continue;
        }
        if (js[i] == ')') {
            if (round_block[round_nesting_level] != ROUND_BLOCK_PARAM_ARROWFUNC_SINGLE) {
                js_min[js_min_length++] = ')';
            }
            if (--round_nesting_level < 0) {
                free(js_min);
                fprintf(stderr, "Unexpected ) in line %d, column %d\n", line, i - last_newline);
                return NULL;
            }
            i += 1;
            continue;
        }
        if (js[i] == ';') {
            if (round_nesting_level > 0 && round_block[round_nesting_level] == ROUND_BLOCK_CONDITION) {
                // Do not remove `;` in `for(;;i++){…}`
                js_min[js_min_length++] = ';';
                i += 1;
                continue;
            }
            char before_semicolon = (js_min_length == 0 ? '}' : js_min[js_min_length - 1]);
            do {
                i += 1;
                skip_whitespaces_comments(js, &i, js_min, &js_min_length, &line, &last_newline, true);
            } while (js[i] == ';');

            // `;` can be removed before `}` and at the end of the document except
            // when it follows the `)` of a condition.

            if (
                (js[i] == '\0' || js[i] == '}') &&
                !(
                    before_semicolon == ')' &&
                    round_block[round_nesting_level + 1] == ROUND_BLOCK_CONDITION
                )
            ) {
                continue;
            }

            js_min[js_min_length++] = ';';
            continue;
        }
        if (js[i] == '/' && js[i + 1] != '/' && js[i + 1] != '*' &&
            (js_min_length == 0 || strchr("^!&|([{><+-*%:?~,;=", js_min[js_min_length - 1]) != NULL))
        {
            // This is a regex object.

            js_min[js_min_length++] = '/';
            i += 1;
            bool active_backslash = false;
            bool in_angular_brackets = false;
            while (js[i] != '\0' && (js[i] != '/' || active_backslash || in_angular_brackets)) {
                js_min[js_min_length++] = js[i];
                if (js[i] == '\\') {
                    active_backslash = !active_backslash;
                }
                else {
                    active_backslash = false;
                }
                if (js[i] == '[' && !active_backslash) {
                    in_angular_brackets = true;
                }
                if (js[i] == ']' && !active_backslash) {
                    in_angular_brackets = false;
                }
                i += 1;
            }
            js_min[js_min_length++] = '/';
            i += 1;
            continue;
        }
        if (js[i] == '`' || js[i] == '"' || js[i] == '\'') {
            char quot = js[i];
            js_min[js_min_length++] = quot;
        merge_quote:
            i += 1;
            bool active_backslash = false;
            while (js[i] != '\0' && (js[i] != quot || active_backslash)) {
                js_min[js_min_length++] = js[i];
                if (js[i] == '\\') {
                    active_backslash = !active_backslash;
                }
                else {
                    active_backslash = false;
                }
                i += 1;
            }
            if (js[i] != quot) {
                free(js_min);
                fprintf(stderr, "Unexpected end of document, expected %c\n", quot);
                return NULL;
            }
            i += 1;
            int k = i;
            bool preserve_comment = skip_whitespaces_comments(js, &k, NULL, NULL, NULL, NULL, true);
            if (!preserve_comment && js[k] == '+') {
                k += 1;
                preserve_comment = skip_whitespaces_comments(js, &k, NULL, NULL, NULL, NULL, true);
                if (!preserve_comment && js[k] == quot) {
                    skip_whitespaces_comments(js, &i, js_min, &js_min_length, &line, &last_newline, true);
                    i += 1;
                    skip_whitespaces_comments(js, &i, js_min, &js_min_length, &line, &last_newline, true);
                    goto merge_quote;
                }
            }
            js_min[js_min_length++] = quot;
            continue;
        }
        if (is_whitespace(js[i]) ||
            js[i] == '/' && js[i + 1] == '*' ||
            js[i] == '/' && js[i + 1] == '/')
        {
            int old_line = line;
            skip_whitespaces_comments(js, &i, js_min, &js_min_length, &line, &last_newline, true);
            if (js_min_length == 0) {
                continue;
            }
            if (js[i] == '}' || js[i] == '\0') {
                continue;
            }

            if (js[i] == '+' && js_min[js_min_length - 1] == '+' ||
                js[i] == '-' && js_min[js_min_length - 1] == '-')
            {
                js_min[js_min_length++] = ' ';
                continue;
            }
            if (
                js_min[js_min_length - 1] == ')' &&
                (
                    round_block[round_nesting_level + 1] == ROUND_BLOCK_CONDITION ||
                    round_block[round_nesting_level + 1] == ROUND_BLOCK_PARAM ||
                    js[i] == '='
                )
            ) {
                continue;
            }
            if (
                js_min[js_min_length - 1] == '}' &&
                curly_block[curly_nesting_level + 1] == CURLY_BLOCK_CONDITION_BODY
            ) {
                continue;
            }
            if (old_line != line) {
                // In JavaScript, a `\n` can end a statement similar to `;`. We only remove `\n` when we are
                // sure that it neither ends a statement nor is required as a white-space between keywords or
                // identifiers. To keep this minifier simple, we accept to miss some occasions were `\n` can
                // be removed.
                //
                // Replacing `\n` between keywords and identifiers by `;` or ` ` would be quite difficult and
                // just cosmetic, therefore we preserve these newlines. How to replace depends not only on
                // the keyword but also on its context. For example, `await` is only a keyword in async
                // functions, `get` and `set` are keywords only in object definition blocks, and `async` and
                // access modifiers can be used as variables inside function blocks. Additionally we would
                // need to consider backward and forward compatibility with different JavaScript versions and
                // with TypeScript.

                const char trim_newline_after[] = ".([{;=*-+^!~?:,><-+'\"/`|&";
                if (strchr(trim_newline_after, js_min[js_min_length - 1]) != NULL) {
                    continue;
                }

                const char trim_newline_before[] = ")]}.;=*-+^!~?:,><-+/|&";
                if (strchr(trim_newline_before, js[i]) != NULL) {
                    continue;
                }
                js_min[js_min_length++] = '\n';
            }
            else {
                const char trim_space_around[] = ".()[]{},=*;?!:><-+'\"/|&`";
                if (strchr(trim_space_around, js[i]) != NULL ||
                    strchr(trim_space_around, js_min[js_min_length - 1]) != NULL)
                {
                    continue;
                }
                js_min[js_min_length++] = ' ';
            }
            continue;
        }
        js_min[js_min_length++] = js[i];
        i += 1;
    }
    if (round_nesting_level != 0) {
        free(js_min);
        fprintf(stderr, "There are unclosed round brackets\n");
        return NULL;
    }
    if (curly_nesting_level != 0) {
        free(js_min);
        fprintf(stderr, "There are unclosed curly brackets\n");
        return NULL;
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
        if (!strncmp(&sgml[i], "<!--", 4)) {
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
                fprintf(stderr, "Invalid `<` in line %d, column %d\n", line, i - last_newline);
                return NULL;
            }
            if (is_whitespace(sgml[i + 1])) {
                free(sgml_min);
                fprintf(stderr, "Invalid whitespace after `<` in line %d, column %d\n",
                        line, i - last_newline);
                return NULL;
            }
            if (sgml[i + 1] == '/' && is_whitespace(sgml[i + 1])) {
                free(sgml_min);
                fprintf(stderr, "Invalid whitespace after `/` in line %d, column %d\n",
                        line, i - last_newline + 1);
                return NULL;
            }
            whitespace_before_tag =
                sgml_min_length > 0 && is_whitespace(sgml_min[sgml_min_length - 1]);
            i += 1;
            if (!strnicmp(&sgml[i], "!DOCTYPE", sizeof "!DOCTYPE" - 1)) {
                syntax_block = SYNTAX_BLOCK_DOCTYPE;
                sgml_min[sgml_min_length++] = '<';
                continue;
            }

            is_closing_tag = (sgml[i] == '/');
            current_tag = &sgml[i];
            current_tag_length = 0;
            while (sgml[i + current_tag_length] != '>' &&
                   sgml[i + current_tag_length] != '\0' &&
                   !is_whitespace(sgml[i + current_tag_length]))
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
                fprintf(stderr, "Invalid `>` in line %d, column %d\n", line, i - last_newline);
                return NULL;
            }
            if (sgml[i - 1] == '/') {
                is_closing_tag = true;
            }

            // Transform `<sgml></sgml>` to `<sgml/>`. This would be illegal for HTML.

            else if (sgml_subset == SGML_SUBSET_XML &&
                     syntax_block != SYNTAX_BLOCK_DOCTYPE &&
                     sgml[i + 1] == '<' && sgml[i + 2] == '/' &&
                     !strncmp(current_tag, &sgml[i + 3], current_tag_length) &&
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
            !strnicmp(current_tag, "script", sizeof "script" - 1))
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
                !strnicmp(current_tag, "pre", sizeof "pre" - 1))
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
            if (!whitespace_before_tag && sgml[i] != '\0') {
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

char *minify_json(const char *json)
{
    char *json_min = malloc(strlen(json) + 1);
    if (json_min == NULL) {
        perror(NULL);
        return NULL;
    }
    int json_min_length = 0;
    int i = 0;
    while (true) {
        if (json[i] == '\0') {
            break;
        }
        while (is_whitespace(json[i])) {
            i += 1;
        }
        if (json[i] == '"') {
            i += 1;
            json_min[json_min_length++] = '"';
            bool active_backslash = false;
            while (json[i] != '\0' && (json[i] != '"' || active_backslash)) {
                if (json[i] == '\\') {
                    active_backslash = !active_backslash;
                }
                else {
                    active_backslash = false;
                }
                json_min[json_min_length++] = json[i];
                i += 1;
            }
            if (json[i] == '\0') {
                free(json_min);
                fprintf(stderr, "Unclosed JSON string\n");
                return NULL;
            }
            json_min[json_min_length++] = '"';
            i += 1;
            continue;
        }
        json_min[json_min_length++] = json[i];
        i += 1;
    }
    char *json_min_realloc = realloc(json_min, json_min_length);
    if (json_min_realloc == NULL) {
        free(json_min);
        perror(NULL);
        return NULL;
    }
    return json_min;
}

static void print_usage()
{
    fputs("Usage: minify <css|js|xml|html|json> <input file|-> [--benchmark]\n", stderr);
}

int main(int argc, char *argv[])
{
    bool benchmark = false;
    const char *format_str = NULL;
    const char *input_filename = NULL;
    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--benchmark")) {
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
        FORMAT_JSON,
    } format;
    if (!strcmp(format_str, "js")) {
        format = FORMAT_JS;
    }
    else if (!strcmp(format_str, "css")) {
        format = FORMAT_CSS;
    }
    else if (!strcmp(format_str, "xml")) {
        format = FORMAT_XML;
    }
    else if (!strcmp(format_str, "html")) {
        format = FORMAT_HTML;
    }
    else if (!strcmp(format_str, "json")) {
        format = FORMAT_JSON;
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
    case FORMAT_JSON:
        input_min = minify_json(input);
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

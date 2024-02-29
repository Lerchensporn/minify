#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

struct Minification
{
    char *result;
    int error_position;
    bool result_is_error_message;
};

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

enum CommentVariant {COMMENT_VARIANT_CSS, COMMENT_VARIANT_JS};

static bool skip_whitespaces_comments(struct Minification *m, const char *input, int *i, char *min,
    int *min_length, enum CommentVariant comment_variant)
{
    bool preserving_comment = false;
    do {
        while (is_whitespace(input[*i])) {
            *i += 1;
        }
        int preserved_comment_start = -1;
        if (input[*i] == '\0') {
            break;
        }
        else if (input[*i] == '/' && input[*i + 1] == '*') {
            int comment_start = *i;
            if (input[*i + 2] == '!') {
                preserved_comment_start = *i;
            }
            *i += 2;
            while (input[*i] != '\0' && (input[*i] != '*' || input[*i + 1] != '/')) {
                *i += 1;
            }
            if (input[*i] == '\0') {
                m->error_position = comment_start;
                asprintf(&m->result, "Unclosed multi-line comment starting in line %%d, column %%d\n");
                m->result_is_error_message = true;
                return false;
            }
            *i += 2;
        }
        else if (comment_variant == COMMENT_VARIANT_JS && input[*i] == '/' && input[*i + 1] == '/') {
            *i += 2;
            while (input[*i] != '\0' && input[*i] != '\n') {
                *i += 1;
            }
        }
        else {
            break;
        }
        if (preserved_comment_start >= 0) {
            preserving_comment = true;
            if (min != NULL) {
                memcpy(&min[*min_length], &input[preserved_comment_start], *i - preserved_comment_start);
                *min_length += *i - preserved_comment_start;
            }
        }
    } while (true);
    return preserving_comment;
}

static int strnicmp(const char *s1, const char *s2, size_t length)
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

struct Minification minify_css(const char *css)
{
    struct Minification m = {0};
    char *css_min = malloc(strlen(css) + 1);
    if (css_min == NULL) {
        m.result = strdup(strerror(errno));
        m.result_is_error_message = true;
        return m;
    }
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

    int css_min_length = 0;
    const char *atrule = NULL;
    int atrule_length;
    int i = 0;
    int nesting_level = -1;

    #define CSS_SKIP_WHITESPACES_COMMENTS(css, ptr_i, css_min, ptr_css_min_length) \
        skip_whitespaces_comments(&m, css, ptr_i, css_min, ptr_css_min_length, COMMENT_VARIANT_CSS); \
        if (m.result_is_error_message) { \
            free(css_min); \
            return m; \
        }

    CSS_SKIP_WHITESPACES_COMMENTS(css, &i, css_min, &css_min_length);
    while (true) {
        if (css[i] == '\0') {
            if (syntax_block != SYNTAX_BLOCK_RULE_START) {
                free(css_min);
                while (i > 0 && is_whitespace(css[i - 1])) {
                    i -= 1;
                }
                char *error;
                if (syntax_block == SYNTAX_BLOCK_STYLE) {
                    error = "Unexpected end of stylesheet, expected `}` after line %%d, column %%d\n";
                }
                else if (syntax_block == SYNTAX_BLOCK_QRULE) {
                    error = "Unexpected end of stylesheet, expected `{…}` after line %%d, column %%d\n";
                }
                else if (syntax_block == SYNTAX_BLOCK_ATRULE) {
                    error = "Unexpected end of stylesheet, expected `;` or `{…}` after line %%d, column %%d\n";
                }
                else {
                    error = "Unexpected end of stylesheet after line %%d, column %%d\n";
                }
                m.error_position = i - 1;
                asprintf(&m.result, error);
                m.result_is_error_message = true;
                return m;
            }
            css_min[css_min_length] = '\0';
            break;
        }
        if (css[i] == '}') {
            do {
                if (nesting_level == -1) {
                    free(css_min);
                    m.error_position = i;
                    asprintf(&m.result, "Unexpected `}` in line %%d, column %%d\n");
                    m.result_is_error_message = true;
                    return m;
                }
                css_min[css_min_length++] = '}';
                nesting_level -= 1;
                i += 1;
                CSS_SKIP_WHITESPACES_COMMENTS(css, &i, css_min, &css_min_length);
            } while (css[i] == '}');
            syntax_block = SYNTAX_BLOCK_RULE_START;
            continue;
        }
        if (syntax_block == SYNTAX_BLOCK_RULE_START) {
            if (css[i] == '{' || css[i] == '}' || css[i] == '"' || css[i] == '\'') {
                free(css_min);
                m.error_position = i;
                asprintf(&m.result, "Unexpected `%c` in line %%d, column %%d\n", css[i]);
                m.result_is_error_message = true;
                return m;
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
        if (i >= 3 && !strncmp(&css[i - 3], "url(", 4)) {
            css_min[css_min_length++] = '(';
            i += 1;
            while (is_whitespace(css[i])) {
                i += 1;
            }
            if (css[i] == '"' || css[i] == '\'') {
                int quot_start_i = i;
                char quot = css[i];
                bool active_backslash = false;
                do {
                    active_backslash = (css[i] == '\\') * !active_backslash;
                    css_min[css_min_length++] = css[i];
                    i += 1;
                } while ((css[i] != quot || active_backslash) && css[i] != '\0');
                if (css[i] == '\0') {
                    free(css_min);
                    m.error_position = quot_start_i;
                    asprintf(&m.result, "Unclosed string starting in line %%d, column %%d\n");
                    m.result_is_error_message = true;
                    return m;
                }
                css_min[css_min_length++] = quot;
                i += 1;
                while (is_whitespace(css[i])) {
                    i += 1;
                }
                if (css[i] != ')') {
                    free(css_min);
                    m.error_position = i;
                    asprintf(&m.result, "Expected `)` in line %%d, column %%d\n");
                    m.result_is_error_message = true;
                    return m;
                }
            }
            else {
                while ((css[i] != ')' || css[i - 1] == '\\') && css[i] != '\0' && !is_whitespace(css[i])) {
                    css_min[css_min_length++] = css[i];
                    i += 1;
                }
                while (is_whitespace(css[i])) {
                    i += 1;
                }
                if (css[i] != ')') {
                    free(css_min);
                    if (css[i] == '\0') {
                        free(css_min);
                        m.error_position = i;
                        asprintf(&m.result, "Unexpected end of stylesheet, expected `)` in line %%d, column %%d\n");
                        m.result_is_error_message = true;
                        return m;
                    }
                    else if (is_whitespace(css[i - 1])) {
                        free(css_min);
                        m.error_position = i;
                        asprintf(&m.result, "Illegal whitespace in URL in line %%d, column %%d\n");
                        m.result_is_error_message = true;
                        return m;
                    }
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
            int quote_start_i = i;
            css_min[css_min_length++] = css[i++];
            bool active_backslash = false;
            while (css[i] != '\0' && (css[i] != css[quote_start_i] || active_backslash)) {
                active_backslash = (css[i] == '\\') * !active_backslash;
                css_min[css_min_length++] = css[i];
                i += 1;
            }
            if (css[i] == '\0') {
                free(css_min);
                m.error_position = quote_start_i;
                asprintf(&m.result, "Unclosed string starting in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            css_min[css_min_length++] = css[quote_start_i];
            i += 1;
            continue;
        }
        if (css[i] == ';' && syntax_block != SYNTAX_BLOCK_QRULE) {
            do {
                i += 1;
                CSS_SKIP_WHITESPACES_COMMENTS(css, &i, css_min, &css_min_length);
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
                m.error_position = i;
                asprintf(&m.result, "Unexpected `{` in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            css_min[css_min_length++] = '{';
            i += 1;
            CSS_SKIP_WHITESPACES_COMMENTS(css, &i, css_min, &css_min_length);
            if (syntax_block == SYNTAX_BLOCK_QRULE) {
                syntax_block = SYNTAX_BLOCK_STYLE;
            }
            else if (syntax_block == SYNTAX_BLOCK_ATRULE) {
                bool is_nestable_atrule =
                    sizeof "@media" - 1 == atrule_length &&
                    !strnicmp(atrule, "@media", atrule_length) ||

                    sizeof "@layer " - 1 == atrule_length &&
                    !strnicmp(atrule, "@layer", atrule_length) ||

                    sizeof "@container" - 1 == atrule_length &&
                    !strnicmp(atrule, "@container", atrule_length) ||

                    sizeof "@keyframes" - 1 == atrule_length &&
                    !strnicmp(atrule, "@keyframes", atrule_length);

                syntax_block = is_nestable_atrule ? SYNTAX_BLOCK_RULE_START : SYNTAX_BLOCK_STYLE;
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
                // Removing whitespace around `:` in `@media (with : 3 px){}` but not in `@page :left{}`

                CSS_SKIP_WHITESPACES_COMMENTS(css, &i, css_min, &css_min_length);
                if (strchr("(,<>:", css_min[css_min_length - 1]) == NULL &&
                    strchr("),<>:", css[i]) == NULL)
                {
                    css_min[css_min_length++] = ' ';
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_ATRULE_SQUARE_BRACKETS ||
                     syntax_block == SYNTAX_BLOCK_QRULE_SQUARE_BRACKETS)
            {
                CSS_SKIP_WHITESPACES_COMMENTS(css, &i, css_min, &css_min_length);
                if (strchr("[=,", css_min[css_min_length - 1]) == NULL &&
                    strchr("]=,*$^-|", css[i]) == NULL)
                {
                    css_min[css_min_length++] = ' ';
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_ATRULE) {
                int before_whitespace = i;
                CSS_SKIP_WHITESPACES_COMMENTS(css, &i, css_min, &css_min_length);

                // Removing whitespace before `(` in `@media (...){}` but not in `@media all and (...){}`

                if ((css[i] != '(' || &atrule[atrule_length - 1] != &css[before_whitespace] - 1) &&
                    strchr(",)(", css_min[css_min_length - 1]) == NULL &&
                    strchr(",);{", css[i]) == NULL)
                {
                    css_min[css_min_length++] = ' ';
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_QRULE) {
                CSS_SKIP_WHITESPACES_COMMENTS(css, &i, css_min, &css_min_length);
                if (strchr("~>+,]", css_min[css_min_length - 1]) == NULL &&
                    strchr("~>+,[{", css[i]) == NULL)
                {
                    css_min[css_min_length++] = ' ';
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_STYLE) {
                CSS_SKIP_WHITESPACES_COMMENTS(css, &i, css_min, &css_min_length);
                if (strchr("{:,", css_min[css_min_length - 1]) == NULL &&
                    strchr("}:,;!", css[i]) == NULL)
                {
                    css_min[css_min_length++] = ' ';
                }
            }
            continue;
        }
        css_min[css_min_length++] = css[i];
        i += 1;
    }
    m.result = realloc(css_min, css_min_length + 1);
    if (m.result == NULL) {
        free(css_min);
        m.result = strdup(strerror(errno));
        m.result_is_error_message = true;
    }
    return m;
}

struct Minification minify_json(const char *json)
{
    struct Minification m = {0};
    char *json_min = malloc(strlen(json) + 1);
    if (json_min == NULL) {
        m.result = strdup(strerror(errno));
        m.result_is_error_message = true;
        return m;
    }
    char bracket_type[512];
    int nesting_level = -1;
    int json_min_length = 0, i = 0;
    while (true) {
        while (is_whitespace(json[i])) {
            i += 1;
        }
        if (json[i] == '\0') {
            json_min[json_min_length] = '\0';
            break;
        }
        if ((json[i] == ',' || json[i] == '}') && json_min[json_min_length - 1] == ':') {
            free(json_min);
            m.error_position = i;
            asprintf(&m.result, "No value after `:` in line %%d, column %%d\n");
            m.result_is_error_message = true;
            return m;
        }
        if (json[i] == '[' || json[i] == '{') {
            if (++nesting_level == sizeof bracket_type / sizeof bracket_type[0]) {
                free(json_min);
                m.error_position = i;
                asprintf(&m.result, "The nesting level is getting too deep in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            bracket_type[nesting_level] = json[i];
            json_min[json_min_length++] = json[i];
            i += 1;
            continue;
        }
        if (json[i] == ']' || json[i] == '}') {
            if (json_min[json_min_length - 1] == ',') {
                free(json_min);
                m.error_position = i;
                asprintf(&m.result, "Illegal `,` before bracket in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            if (nesting_level == -1 || bracket_type[nesting_level] != json[i] - 2) {
                free(json_min);
                m.error_position = i;
                asprintf(&m.result, "Unexpected `%c` in line %%d, column %%d\n", json[i]);
                m.result_is_error_message = true;
                return m;
            }
            nesting_level -= 1;
            json_min[json_min_length++] = json[i];
            i += 1;
            continue;
        }

        bool is_key = nesting_level >= 0 && (bracket_type[nesting_level] == '{' &&
            (json_min[json_min_length - 1] == ',' || json_min[json_min_length - 1] == '{'));

        if (json[i] == '"') {
            i += 1;
            json_min[json_min_length++] = '"';
            bool active_backslash = false;
            while (json[i] != '\0' && (json[i] != '"' || active_backslash)) {
                if (json[i] == '\n') {
                    free(json_min);
                    m.error_position = i - 1;
                    asprintf(&m.result, "Illegal line break in JSON string after line %d\n");
                    m.result_is_error_message = true;
                    return m;
                }
                active_backslash = (json[i] == '\\') * !active_backslash;
                if (active_backslash && strchr("\"\\/bfnrtu", json[i + 1]) == NULL) {
                    free(json_min);
                    m.error_position = i;
                    asprintf(&m.result, "Invalid JSON escape sequence `\\%c` in line %%d, column %%d\n",
                        json[i + 1]);
                    m.result_is_error_message = true;
                    return m;
                }
                if (active_backslash && json[i + 1] == 'u') {
                    bool invalid_unicode = false;
                    int k;
                    for (k = i + 2; k <= i + 5; ++k) {
                        if (!(
                            (json[k] >= '0' && json[k] <= '9') || json[k] >= 'a' && json[k] <= 'f' ||
                            json[k] >= 'A' && json[k] <= 'F'
                        )) {
                            invalid_unicode = true;
                        }
                        if (json[k] == '"' || json[k] == '\n' || json[k] == '\0') {
                            break;
                        }
                    }
                    if (invalid_unicode) {
                        free(json_min);
                        m.error_position = i - 1;
                        asprintf(&m.result, "Invalid JSON escape sequence `%.*s` in line %%d, column %%d\n",
                            k - i, &json[i]);
                        m.result_is_error_message = true;
                        return m;
                    }
                }
                json_min[json_min_length++] = json[i];
                i += 1;
            }
            if (json[i] == '\0') {
                free(json_min);
                m.error_position = i - 1;
                asprintf(&m.result, "Unexpected end of JSON document, expected `\"` after line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            json_min[json_min_length++] = '"';
            i += 1;
            if (!is_key) {
                continue;
            }
            while (is_whitespace(json[i])) {
                i += 1;
            }
            if (json[i] != ':') {
                free(json_min);
                m.error_position = i;
                asprintf(&m.result, "Expected `:` instead of `%c` in line %%d, column %%d\n", json[i]);
                m.result_is_error_message = true;
                return m;
            }
            json_min[json_min_length++] = ':';
            i += 1;
            continue;
        }

        if (is_key) {
            free(json_min);
            m.error_position = i;
            asprintf(&m.result, "Expected `\"`, `[` or `{` instead of `%c` in line %%d, column %%d\n",
                json[i]);
            m.result_is_error_message = true;
            return m;
        }

        if (!strncmp(&json[i], "true", sizeof "true" - 1) &&
            (json[i + sizeof "true" - 1] == '\0' || strchr(" \r\t\n],}", json[i + sizeof "true" - 1])))
        {
            strcpy(&json_min[json_min_length], "true");
            json_min_length += sizeof "true" - 1;
            i += sizeof "true" - 1;
            continue;
        }
        if (!strncmp(&json[i], "false", sizeof "false" - 1) &&
            (json[i + sizeof "false" - 1] == '\0' || strchr("\r\t\n],}", json[i + sizeof "false" - 1])))
        {
            strcpy(&json_min[json_min_length], "false");
            json_min_length += sizeof "false" - 1;
            i += sizeof "false" - 1;
            continue;
        }
        if (!strncmp(&json[i], "null", sizeof "null" - 1) &&
            (json[i + sizeof "null" - 1] == '\0' || strchr("\r\t\n],}", json[i + sizeof "null" - 1])))
        {
            strcpy(&json_min[json_min_length], "null");
            json_min_length += sizeof "null" - 1;
            i += sizeof "null" - 1;
            continue;
        }
        if (json[i] >= '0' && json[i] <= '9') {
            int k = i;
            while (json[k] >= '0' && json[k] <= '9') {
                k += 1;
            }
            if (json[k] == '.') {
                k += 1;
            }
            while (json[k] >= '0' && json[k] <= '9') {
                k += 1;
            }
            if ((json[k] == 'e' || json[k] == 'E') && (json[k + 1] == '+' || json[k + 1] == '-') &&
                json[k + 2] >= '0' && json[k + 2] <= '9')
            {
                k += 2;
            }
            while (json[k] >= '0' && json[k] <= '9') {
                k += 1;
            }
            memcpy(&json_min[json_min_length], &json[i], k - i);
            json_min_length += k - i;
            i = k;
            continue;
        }
        if (nesting_level >= 0 && json[i] == ',' && json_min[json_min_length - 1] != ',') {
            json_min[json_min_length++] = ',';
            i += 1;
            continue;
        }
        if (json[i] != '\0' && !is_whitespace(json[i])) {
            free(json_min);
            m.error_position = i;
            asprintf(&m.result, "Unexpected data starting with `%c` in line %%d, column %%d\n", json[i]);
            m.result_is_error_message = true;
            return m;
        }
    }
    if (nesting_level != -1) {
        free(json_min);
        do {
            i -= 1;
        } while (is_whitespace(json[i]));
        m.error_position = i;
        asprintf(&m.result, "Missing `%c` after line %%d, column %%d\n", bracket_type[nesting_level]);
        m.result_is_error_message = true;
        return m;
    }
    m.result = realloc(json_min, json_min_length + 1);
    if (m.result == NULL) {
        free(json_min);
        m.result = strdup(strerror(errno));
        m.result_is_error_message = true;
    }
    return m;
}

struct Minification minify_js(const char *js)
{
    struct Minification m = {0};
    char *js_min = malloc(strlen(js) + 1);
    if (js_min == NULL) {
        m.result = strdup(strerror(errno));
        m.result_is_error_message = true;
        return m;
    }
    int js_min_length = 0;
    int i = 0;
    int last_open_curly_bracket_i, last_open_round_bracket_i;

    const char *identifier_delimiters = "'\"`%<>+*/-=,(){}[]!~;|&^:? \t\r\n";

    enum {
        CURLY_BLOCK_UNKNOWN,
        CURLY_BLOCK_STANDALONE,
        CURLY_BLOCK_FUNC_BODY,
        CURLY_BLOCK_FUNC_BODY_STANDALONE,
        CURLY_BLOCK_CONDITION_BODY,
        CURLY_BLOCK_ARROWFUNC_BODY,
    } curly_block[64];
    int curly_nesting_level = -1;

    enum {
        ROUND_BLOCK_UNKNOWN,
        ROUND_BLOCK_CONDITION,
        ROUND_BLOCK_CATCH_SWITCH,
        ROUND_BLOCK_PARAM,
        ROUND_BLOCK_PARAM_STANDALONE,
        ROUND_BLOCK_PARAM_ARROWFUNC_SINGLE,
    } round_block[64];
    int round_nesting_level = -1;

    #define JS_SKIP_WHITESPACES_COMMENTS(js, ptr_i, js_min, ptr_js_min_length) \
        skip_whitespaces_comments(&m, js, ptr_i, js_min, ptr_js_min_length, COMMENT_VARIANT_JS); \
        if (m.result_is_error_message) { \
            free(js_min); \
            return m; \
        }

    while (true) {
        if (js[i] == '\0') {
            js_min[js_min_length] = '\0';
            break;
        }

        int next_word_length = strcspn(&js[i], identifier_delimiters);
        if (next_word_length == 0) {
            goto after_keywords;
        }

        // Keywords lose their meaning when used as object keys

        {
            int k = i + next_word_length;
            JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
            if (js[k] == ':') {
                memcpy(&js_min[js_min_length], &js[i], next_word_length);
                js_min_length += next_word_length;
                i += next_word_length;
                continue;
            }
        }

        // Next we handle keywords

        if (next_word_length == sizeof "switch" - 1 && !strncmp(&js[i], "switch", next_word_length) ||
            next_word_length == sizeof "catch" - 1 && !strncmp(&js[i], "catch", next_word_length))
        {
            memcpy(&js_min[js_min_length], &js[i], next_word_length);
            i += next_word_length;
            js_min_length += next_word_length;

            JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);
            if (js[i] == '(') {
                if (++round_nesting_level == sizeof round_block / sizeof round_block[0]) {
                    free(js_min);
                    m.error_position = i;
                    asprintf(&m.result,
                        "The nesting level of round brackets is getting too deep in line %%d, column %%d\n");
                    m.result_is_error_message = true;
                    return m;
                }
                last_open_round_bracket_i = i;
                round_block[round_nesting_level] = ROUND_BLOCK_CATCH_SWITCH;
                js_min[js_min_length++] = '(';
                i += 1;
            }
            else if (js[i] == '{') {
                if (++curly_nesting_level == sizeof curly_block / sizeof curly_block[0]) {
                    free(js_min);
                    m.error_position = i;
                    asprintf(&m.result,
                        "The nesting level of curly brackets is getting too deep in line %%d, column %%d\n");
                    m.result_is_error_message = true;
                    return m;
                }
                last_open_curly_bracket_i = i;
                curly_block[curly_nesting_level] = CURLY_BLOCK_CONDITION_BODY;
                js_min[js_min_length++] = '{';
                i += 1;
            }
            else {
                free(js_min);
                m.error_position = i;
                asprintf(&m.result, "Expected `(` or `{` in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            continue;
        }
        if (
            next_word_length == sizeof "do" - 1 && !strncmp(&js[i], "do", next_word_length) ||
            next_word_length == sizeof "try" - 1 && !strncmp(&js[i], "try", next_word_length) ||
            next_word_length == sizeof "finally" - 1 && !strncmp(&js[i], "finally", next_word_length)
        ) {
            memcpy(&js_min[js_min_length], &js[i], next_word_length);
            i += next_word_length;
            js_min_length += next_word_length;

            JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);
            if (js[i] == '{') {
                if (++curly_nesting_level == sizeof curly_block / sizeof curly_block[0]) {
                    free(js_min);
                    m.error_position = i;
                    asprintf(&m.result,
                        "The nesting level of curly brackets is getting too deep in line %%d, column %%d\n");
                    m.result_is_error_message = true;
                    return m;
                }
                last_open_curly_bracket_i = i;
                curly_block[curly_nesting_level] = CURLY_BLOCK_CONDITION_BODY;
                js_min[js_min_length++] = '{';
                i += 1;
            }
            else if (next_word_length == sizeof "do" - 1) {
                if (strchr(identifier_delimiters, js[i]) == NULL) {
                    js_min[js_min_length++] = ' ';
                }
            }
            else {
                free(js_min);
                m.error_position = i;
                asprintf(&m.result, "Expected `{` in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            continue;
        }
        if (next_word_length == sizeof "function" - 1 && !strncmp(&js[i], "function", next_word_length)) {
            // We consume the input until `(` of the parameter list.
            //
            // Regular functions cannot be safely replaced by arrow functions.  Arrow functions
            // cannot be used as constructors: `new arrow_function()` where `arrow_function` is an
            // arrow function is invalid.

            bool standalone =
                js_min_length == 0 ||
                js_min[js_min_length - 1] == ';' ||
                js_min[js_min_length - 1] == '}' ||
                js_min[js_min_length - 1] == '\n';

            memcpy(&js_min[js_min_length], &js[i], next_word_length);
            i += next_word_length;
            js_min_length += next_word_length;

            JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);

            if (js[i] == '*') {
                js_min[js_min_length++] = '*';
                i += 1;
                JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);
            }
            if (js[i] != '(') {
                js_min[js_min_length++] = ' ';
                while (strchr(identifier_delimiters, js[i]) == NULL) {
                    js_min[js_min_length++] = js[i++];
                }
            }
            JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);
            if (js[i] != '(') {
                free(js_min);
                m.error_position = i; 
                asprintf(&m.result, "Expected `(` in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            if (++round_nesting_level == sizeof round_block / sizeof round_block[0]) {
                free(js_min);
                m.error_position = i;
                asprintf(&m.result,
                    "The nesting level of round brackets is getting too deep in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            last_open_round_bracket_i = i;
            round_block[round_nesting_level] = standalone ? ROUND_BLOCK_PARAM_STANDALONE : ROUND_BLOCK_PARAM;
            js_min[js_min_length++] = '(';
            i += 1;
            continue;
        }
        if (
            next_word_length == sizeof "if" - 1 && !strncmp(&js[i], "if", next_word_length) ||
            next_word_length == sizeof "for" - 1 && !strncmp(&js[i], "for", next_word_length) ||
            next_word_length == sizeof "while" - 1 && !strncmp(&js[i], "while", next_word_length)
        ) {
            memcpy(&js_min[js_min_length], &js[i], next_word_length);
            i += next_word_length;
            js_min_length += next_word_length;
            JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);
            if (js[i] != '(') {
                free(js_min);
                m.error_position = i;
                asprintf(&m.result, "Expected `(` in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            if (++round_nesting_level == sizeof round_block / sizeof round_block[0]) {
                free(js_min);
                m.error_position = i;
                asprintf(&m.result,
                    "The nesting level of round brackets is getting too deep in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            last_open_round_bracket_i = i;
            i += 1;
            round_block[round_nesting_level] = ROUND_BLOCK_CONDITION;
            js_min[js_min_length++] = '(';
            continue;
        }
        if (next_word_length == sizeof "else" - 1 && !strncmp(&js[i], "else", next_word_length)) {
            memcpy(&js_min[js_min_length], &js[i], next_word_length);
            i += next_word_length;
            js_min_length += next_word_length;
            int k = i;
            JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
            if (js[k] != '{') {
                continue;
            }
            JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);
            i += 1;
            k = i;
            bool preserved_comment = JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
            if (!preserved_comment && js[k] == '}') {
                JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);
                js_min[js_min_length++] = ';';
                do {
                    i += 1;
                    JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);
                } while (js[i] == ';');
            }
            else {
                if (++curly_nesting_level == sizeof curly_block / sizeof curly_block[0]) {
                    free(js_min);
                    asprintf(&m.result,
                        "The nesting level of curly brackets is getting too deep in line %%d, column %%d\n");
                    m.result_is_error_message = true;
                    return m;
                }
                last_open_curly_bracket_i = i;
                js_min[js_min_length++] = '{';
                curly_block[curly_nesting_level] = CURLY_BLOCK_CONDITION_BODY;
            }
            continue;
        }
        // Cannot make this replacement because it is possible to redefine `undefined` in the local scope.
        // if (next_word_length == sizeof "undefined" - 1 && !strncmp(&js[i], "undefined", next_word_length)) {
        //    strcpy(&js_min[js_min_length], "void 0");
        //    i += next_word_length;
        //    js_min_length += sizeof "void 0" - 1;
        //    continue;
        //}
        if (next_word_length == sizeof "true" - 1 && !strncmp(&js[i], "true", next_word_length) ||
            next_word_length == sizeof "false" - 1 && !strncmp(&js[i], "false", next_word_length))
        {
            if (js_min_length > 0 && js_min[js_min_length - 1] == ' ') {
                js_min_length -= 1;
            }
            js_min[js_min_length++] = '!';
            js_min[js_min_length++] = js[i] == 't' ? '0' : '1';
            i += next_word_length;
            continue;
        }

        memcpy(&js_min[js_min_length], &js[i], next_word_length);
        i += next_word_length;
        js_min_length += next_word_length;

    after_keywords:

        if (js[i] == '{') {
            if (++curly_nesting_level == sizeof curly_block / sizeof curly_block[0]) {
                free(js_min);
                asprintf(&m.result,
                    "The nesting level of curly brackets is getting too deep in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            last_open_curly_bracket_i = i;
            i += 1;
            if (js_min_length > 0 && js_min[js_min_length - 1] == ')' &&
                round_block[round_nesting_level + 1] == ROUND_BLOCK_CONDITION)
            {
                // Replacing `if(1){}` by `if(1);`
                bool preserved_comment = JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);
                if (!preserved_comment && js[i] == '}') {
                    js_min[js_min_length++] = ';';
                    do {
                        i += 1;
                        JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);
                    } while (js[i] == ';');
                    curly_nesting_level -= 1;
                    continue;
                }
                else {
                    curly_block[curly_nesting_level] = CURLY_BLOCK_CONDITION_BODY;
                }
            }
            else if (js_min_length >= 1 && js_min[js_min_length - 1] == ')' &&
                     round_block[round_nesting_level + 1] == ROUND_BLOCK_CATCH_SWITCH)
            {
                curly_block[curly_nesting_level] = CURLY_BLOCK_CONDITION_BODY;
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
            else if (js_min_length >= 1 && js_min[js_min_length - 1] == ')' &&
                     round_block[round_nesting_level + 1] == ROUND_BLOCK_PARAM_STANDALONE)
            {
                curly_block[curly_nesting_level] = CURLY_BLOCK_FUNC_BODY_STANDALONE;
            }
            else if (
                js_min_length >= 1 &&
                (
                    js_min[js_min_length - 1] == '}' ||
                    js_min[js_min_length - 1] == ';' ||
                    js_min[js_min_length - 1] == '{' ||
                    js_min[js_min_length - 1] == '\n'
                )
            ) {
                curly_block[curly_nesting_level] = CURLY_BLOCK_STANDALONE;
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
                m.error_position = i;
                asprintf(&m.result,
                    "The nesting level of round brackets is getting too deep in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            last_open_round_bracket_i = i;

            i += 1;

            // Removing round brackets around single parameters with no default
            // value of arrow functions.

            bool remove_round_brackets_around_param = false;

            int k = i;
            JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
            if (js[k] != '.') { // Can't remove round brackets in `(...arg)=>{}`
                int arg_start = k;
                while (strchr(identifier_delimiters, js[k]) == NULL) {
                    k += 1;
                }
                JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
                if (k > arg_start && js[k] == ')') {
                    k += 1;
                    JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
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
            if (--curly_nesting_level < -1) {
                free(js_min);
                m.error_position = i;
                asprintf(&m.result, "Unexpected `}` in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            js_min[js_min_length++] = '}';
            i += 1;
            continue;
        }
        if (js[i] == ')') {
            if (round_block[round_nesting_level] != ROUND_BLOCK_PARAM_ARROWFUNC_SINGLE) {
                js_min[js_min_length++] = ')';
            }
            if (--round_nesting_level < -1) {
                free(js_min);
                m.error_position = i;
                asprintf(&m.result, "Unexpected `)` in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            i += 1;
            continue;
        }
        if (js[i] == ';') {
            if (js_min_length == 0) {
                i += 1;
                continue;
            }
            if (round_nesting_level >= 0 && round_block[round_nesting_level] == ROUND_BLOCK_CONDITION) {
                // Do not remove `;` in `for(;;i++){…}`
                js_min[js_min_length++] = ';';
                i += 1;
                continue;
            }
            char before_semicolon = js_min[js_min_length - 1];
            do {
                i += 1;
                JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);
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

            if (
                before_semicolon == '}' &&
                (
                    curly_block[curly_nesting_level + 1] == CURLY_BLOCK_FUNC_BODY_STANDALONE ||
                    curly_block[curly_nesting_level + 1] == CURLY_BLOCK_STANDALONE
                )
            ) {
                continue;
            }

            js_min[js_min_length++] = ';';
            continue;
        }
        if (js[i] == '/' && js[i + 1] != '/' && js[i + 1] != '*' &&
            (js_min_length == 0 || strchr("^!&|([{><+-*%:?~,;=", js_min[js_min_length - 1]) != NULL ||
            js_min[js_min_length - 1] == ' ' && js_min[js_min_length - 2] == '<'))
        {
            // This is a regex object.

            int regex_start_i = i;
            js_min[js_min_length++] = '/';
            i += 1;
            bool active_backslash = false;
            bool in_angular_brackets = false;
            while (js[i] != '\0' && (js[i] != '/' || active_backslash || in_angular_brackets)) {
                if (js[i] == '\n') {
                    free(js_min);
                    m.error_position = i - 1;
                    asprintf(&m.result, "Illegal line break in regex at the end of line %d\n");
                    m.result_is_error_message = true;
                    return m;
                }
                js_min[js_min_length++] = js[i];
                if (js[i] == '[' && !active_backslash) {
                    in_angular_brackets = true;
                }
                else if (js[i] == ']' && !active_backslash) {
                    in_angular_brackets = false;
                }
                active_backslash = (js[i++] == '\\') * !active_backslash;
            }
            if (js[i] != '/') {
                free(js_min);
                m.error_position = regex_start_i;
                asprintf(&m.result, "Unclosed regex starting in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            js_min[js_min_length++] = '/';
            i += 1;
            continue;
        }
        if (js[i] == '`' || js[i] == '"' || js[i] == '\'') {
            js_min[js_min_length++] = js[i];
        merge_strings:
            int quote_i = i;
            i += 1;
            bool active_backslash = false;
            while (js[i] != '\0' && (js[i] != js[quote_i] || active_backslash)) {
                if (js[i] == '\n') {
                    if (active_backslash) {
                        i += 1;
                        js_min_length -= 1;
                        continue;
                    }
                    else if (js[quote_i] != '`') {
                        free(js_min);
                        m.error_position = i;
                        asprintf(&m.result, "String contains unescaped line break in line %%d, column %%d\n");
                        m.result_is_error_message = true;
                        return m;
                    }
                }
                js_min[js_min_length++] = js[i];
                if (i < quote_i + sizeof "</script" - 1 &&
                    !strnicmp(&js_min[js_min_length - sizeof "</script" + 1], "</script", sizeof "</script" - 1))
                {
                    strcpy(&js_min[js_min_length - sizeof "</script" + 1], "<\\/script");
                    js_min_length += 1;
                }
                active_backslash = (js[i++] == '\\') * !active_backslash;
            }
            if (js[i] != js[quote_i]) {
                free(js_min);
                m.error_position = i - 1;
                asprintf(&m.result,
                    "Unexpected end of script, expected `%c` after line %%d, column %%d\n", js[quote_i]);
                m.result_is_error_message = true;
                return m;
            }
            i += 1;
            int k = i;
            bool preserving_comment = JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
            if (preserving_comment || js[k] != '+') {
                js_min[js_min_length++] = js[quote_i];
                continue;
            }
            k += 1;
            preserving_comment = JS_SKIP_WHITESPACES_COMMENTS(js, &k, NULL, NULL);
            if (preserving_comment || js[k] != js[quote_i]) {
                js_min[js_min_length++] = js[quote_i];
                continue;
            }

            JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);
            i += 1; // Skipping the plus character
            JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);

            goto merge_strings;
        }
        if (is_whitespace(js[i]) ||
            js[i] == '/' && js[i + 1] == '*' ||
            js[i] == '/' && js[i + 1] == '/')
        {
            int whitespace_comment_i = i;
            JS_SKIP_WHITESPACES_COMMENTS(js, &i, js_min, &js_min_length);
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
                    round_block[round_nesting_level + 1] == ROUND_BLOCK_PARAM_STANDALONE ||
                    js[i] == '='
                )
            ) {
                continue;
            }
            if (
                js_min[js_min_length - 1] == '}' &&
                (
                    curly_block[curly_nesting_level + 1] == CURLY_BLOCK_CONDITION_BODY ||
                    curly_block[curly_nesting_level + 1] == CURLY_BLOCK_FUNC_BODY_STANDALONE ||
                    curly_block[curly_nesting_level + 1] == CURLY_BLOCK_STANDALONE
                )
            ) {
                continue;
            }

            // Newlines terminate a preceding statement even when they are in a comment.
            // Try it out: `Math.sin(1)/*\n*/Math.sin(1)` is valid; without `\n` it is invalid.

            bool has_line_break = false;
            do {
                if (js[whitespace_comment_i++] == '\n') {
                    has_line_break = true;
                    break;
                }
            } while (whitespace_comment_i < i);

            if (has_line_break) {
                // In JavaScript, `\n` can end a statement similar to `;`. We only remove `\n` when we are
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

                const char trim_newline_after[] = ".([{;=*-+^!~?:,><-+/|&";
                if (strchr(trim_newline_after, js_min[js_min_length - 1]) != NULL) {
                    continue;
                }

                // Standalone lines may start with: +-~!"'`/ and more

                const char trim_newline_before[] = ")]}.;=*^?:,><|&";
                if (strchr(trim_newline_before, js[i]) == NULL) {
                    js_min[js_min_length++] = '\n';
                }
            }
            else {
                // Minifying a smaller-than comparison with a specific regex may produce a `</script` tag that
                // breaks inline JavaScript in HTML. The HTML minifier could recognize such cases but does
                // not know how to escape it correctly. It is nontrivial to distinguish such a smaller-than
                // regex from a `</script` tag created by merging strings. Therefore this is the right place
                // to handle the issue.

                const char trim_space_around[] = ".()[]{},=*;?!:><-+'\"/|&`";
                if (strchr(trim_space_around, js[i]) == NULL &&
                    strchr(trim_space_around, js_min[js_min_length - 1]) == NULL ||
                    js_min[js_min_length - 1] == '<' && !strnicmp(&js[i], "/script", sizeof "/script" - 1))
                {
                    js_min[js_min_length++] = ' ';
                }
            }
            continue;
        }
        js_min[js_min_length++] = js[i];
        i += 1;
    }
    if (round_nesting_level != -1) {
        free(js_min);
        m.error_position = last_open_round_bracket_i;
        asprintf(&m.result, "Unclosed round bracket in line %%d, column %%d\n");
        m.result_is_error_message = true;
        return m;
    }
    if (curly_nesting_level != -1) {
        free(js_min);
        m.error_position = last_open_curly_bracket_i;
        asprintf(&m.result, "Unclosed curly bracket in line %%d, column %%d\n");
        m.result_is_error_message = true;
        return m;
    }
    m.result = realloc(js_min, js_min_length + 1);
    if (m.result == NULL) {
        free(js_min);
        m.result = strdup(strerror(errno));
        m.result_is_error_message = true;
    }
    return m;
}

enum SGMLSubset {SGML_SUBSET_XML, SGML_SUBSET_HTML};

static void sgml_correct_error_position(const char *encoded, const char *decoded, int *error_position,
    enum SGMLSubset sgml_subset)
{
    int encoded_i = 0, decoded_i = 0;
    bool in_cdata = false;
    int (*tagncmp)(const char *, const char *, size_t);
    tagncmp = (sgml_subset == SGML_SUBSET_XML ? strncmp : strnicmp);
    while (true) {
        if (*error_position == decoded_i) {
            *error_position = encoded_i;
            return;
        }
        if (encoded[encoded_i] == '\0') {
            return;
        }
        if (!in_cdata) {
            if (sgml_subset == SGML_SUBSET_XML &&
                !tagncmp(&encoded[encoded_i], "<![CDATA[", sizeof "<![CDATA[" - 1))
            {
                in_cdata = true;
                encoded_i += sizeof "<![CDATA[" - 1;
                decoded_i += 1;
                continue;
            }
            if (!tagncmp(&encoded[encoded_i], "&lt;", sizeof "&lt;" - 1)) {
                encoded_i += sizeof "&lt;" - 1;
                decoded_i += 1;
                continue;
            }
            if (!tagncmp(&encoded[encoded_i], "&gt;", sizeof "&gt;" - 1)) {
                encoded_i += sizeof "&gt;" - 1;
                decoded_i += 1;
                continue;
            }
            if (!tagncmp(&encoded[encoded_i], "&amp;", sizeof "&amp;" - 1)) {
                encoded_i += sizeof "&amp;" - 1;
                decoded_i += 1;
                continue;
            }
            if (!tagncmp(&encoded[encoded_i], "&apos;", sizeof "&apos;" - 1)) {
                encoded_i += sizeof "&apos;" - 1;
                decoded_i += 1;
                continue;
            }
            if (!tagncmp(&encoded[encoded_i], "&quot;", sizeof "&quot;" - 1)) {
                encoded_i += sizeof "&quot;" - 1;
                decoded_i += 1;
                continue;
            }
            if (sgml_subset == SGML_SUBSET_HTML) {
                if (!tagncmp(&encoded[encoded_i], "&plus;", sizeof "&plus;" - 1)) {
                    encoded_i += sizeof "&quot;" - 1;
                    decoded_i += 1;
                    continue;
                }
                if (!tagncmp(&encoded[encoded_i], "&sol;", sizeof "&sol;" - 1)) {
                    encoded_i += sizeof "&sol;" - 1;
                    decoded_i += 1;
                    continue;
                }
            }
            if (encoded[encoded_i] == '&' && encoded[encoded_i + 1] == '#') {
                encoded_i += 2;
                if (encoded[encoded_i] == 'x' || sgml_subset == SGML_SUBSET_HTML && encoded[encoded_i] == 'X') {
                    encoded_i += 1;
                }
                while (encoded[encoded_i] != ';' && encoded[encoded_i] != '\0') {
                    encoded_i += 1;
                }
                decoded_i += 1;
                continue;
            }
        }
        else if (sgml_subset == SGML_SUBSET_XML && !strncmp(&encoded[encoded_i], "]]>", sizeof "]]>" - 1)) {
            in_cdata = false;
            encoded_i += sizeof "]]>" - 1;
            decoded_i += sizeof "]]>" - 1;
            continue;
        }
        encoded_i += 1;
        decoded_i += 1;
    }
}

static struct Minification sgml_decode(const char *input, int length, enum SGMLSubset sgml_subset)
{
    // This function helps minify inline scripts and styles in XML (e.g. SVG, MathML, XHTML)
    // documents. We need to decode XML entities and CDATA sections before feeding the tag content
    // into the JavaScript or CSS minifier. Note that XML indeed has only five named entities. In
    // HTML, entities are not effective inside script and style tags and CDATA sections are not
    // supported at all.
    //
    // The routine is based on the assumption that decoding will never make the string longer.
    //
    // The implementation of HTML decoding has only limited capability here, just enough to handle possible
    // encoding of the script type attribute.

    int i = 0;
    int result_length = 0;
    bool in_cdata = false;
    struct Minification m = {0};
    int (*tagncmp)(const char *, const char *, size_t);
    tagncmp = (sgml_subset == SGML_SUBSET_XML ? strncmp : strnicmp);
    char *result = malloc(length + 1);
    if (result == NULL) {
        m.result = strdup(strerror(errno));
        m.result_is_error_message = true;
        return m;
    }
    while (i < length) {
        if (!in_cdata) {
            if (sgml_subset == SGML_SUBSET_XML &&
                !strncmp(&input[i], "<![CDATA[", sizeof "<![CDATA[" - 1))
            {
                in_cdata = true;
                i += sizeof "<![CDATA[" - 1;
                continue;
            }
            if (!strncmp(&input[i], "&lt;", sizeof "&lt;" - 1)) {
                result[result_length++] = '<';
                i += sizeof "&lt;" - 1;
                continue;
            }
            if (!strncmp(&input[i], "&gt;", sizeof "&gt;" - 1)) {
                result[result_length++] = '>';
                i += sizeof "&gt;" - 1;
                continue;
            }
            if (!strncmp(&input[i], "&amp;", sizeof "&amp;" - 1)) {
                result[result_length++] = '&';
                i += sizeof "&amp;" - 1;
                continue;
            }
            if (!strncmp(&input[i], "&apos;", sizeof "&apos;" - 1)) {
                result[result_length++] = '\'';
                i += sizeof "&apos;" - 1;
                continue;
            }
            if (!strncmp(&input[i], "&quot;", sizeof "&quot;" - 1)) {
                result[result_length++] = '"';
                i += sizeof "&quot;" - 1;
                continue;
            }
            if (sgml_subset == SGML_SUBSET_HTML) {
                if (!strncmp(&input[i], "&plus;", sizeof "&plus;" - 1)) {
                    result[result_length++] = '+';
                    i += sizeof "&plus;" - 1;
                    continue;
                }
                if (!strncmp(&input[i], "&sol;", sizeof "&sol;" - 1)) {
                    result[result_length++] = '/';
                    i += sizeof "&sol;" - 1;
                    continue;
                }
            }
            if (input[i] == '&' && input[i + 1] == '#') {
                int codepoint = 0;
                int k;
                if (input[i + 2] == 'x' || sgml_subset == SGML_SUBSET_HTML && input[i + 2] == 'X') {
                    for (k = 3; input[i + k] != ';'; ++k) {
                        if (input[i + k] >= '0' && input[i + k] <= '9') {
                            codepoint = codepoint * 16 + (input[i + k] - '0');
                        }
                        else if (input[i + k] >= 'A' && input[i + k] <= 'F') {
                            codepoint = codepoint * 16 + (10 + input[i + k] - 'A');
                        }
                        else if (input[i + k] >= 'a' && input[i + k] <= 'f') {
                            codepoint = codepoint * 16 + (10 + input[i + k] - 'a');
                        }
                        else {
                            codepoint = -1;
                            break;
                        }
                    }
                }
                else {
                    for (k = 2; input[i + k] != ';'; ++k) {
                        if (input[i + k] >= '0' && input[i + k] <= '9') {
                            codepoint = codepoint * 10 + (input[i + k] - '0');
                        }
                        else {
                            codepoint = -1;
                            break;
                        }
                    }
                }
                if (codepoint > 0x7FFFFFFF) {
                    codepoint = -1;
                }
                if (codepoint == -1) {
                    free(result);
                    m.error_position = i;
                    asprintf(&m.result, "XML entity with invalid codepoint in line %%d, column %%d\n");
                    m.result_is_error_message = true;
                    return m;
                }

                i += k + 1;

                // See `man utf-8`

                if (codepoint <= 0x7F) {
                    result[result_length++] = codepoint;
                }
                else if (codepoint <= 0x7FF) {
                    result[result_length++] = 0b11000000 + (codepoint >> 6);
                    result[result_length++] = (10 << 6) + (codepoint & 0b111111);
                }
                else if (codepoint <= 0xFFFF) {
                    result[result_length++] = 0b11100000 + (codepoint >> 12);
                    result[result_length++] = (10 << 6) + ((codepoint >> 6) & 0b111111);
                    result[result_length++] = (10 << 6) + (codepoint & 0b111111);
                }
                else if (codepoint <= 0x1FFFFF) {
                    result[result_length++] = 0b11110000 + (codepoint >> 18);
                    result[result_length++] = (10 << 6) + ((codepoint >> 12) & 0b111111);
                    result[result_length++] = (10 << 6) + ((codepoint >> 6) & 0b111111);
                    result[result_length++] = (10 << 6) + (codepoint & 0b111111);
                }
                else if (codepoint <= 0x03FFFFFF) {
                    result[result_length++] = 0b11111000 + (codepoint >> 24);
                    result[result_length++] = (10 << 6) + ((codepoint >> 18) & 0b111111);
                    result[result_length++] = (10 << 6) + ((codepoint >> 12) & 0b111111);
                    result[result_length++] = (10 << 6) + ((codepoint >> 6) & 0b111111);
                    result[result_length++] = (10 << 6) + (codepoint & 0b111111);
                }
                else if (codepoint <= 0x7FFFFFFF) {
                    result[result_length++] = 0b1111110 + (codepoint >> 30);
                    result[result_length++] = (10 << 6) + ((codepoint >> 24) & 0b111111);
                    result[result_length++] = (10 << 6) + ((codepoint >> 18) & 0b111111);
                    result[result_length++] = (10 << 6) + ((codepoint >> 12) & 0b111111);
                    result[result_length++] = (10 << 6) + ((codepoint >> 6) & 0b111111);
                    result[result_length++] = (10 << 6) + (codepoint & 0b111111);
                }
                continue;
            }
            if (sgml_subset == SGML_SUBSET_XML && input[i] == '&') {
                free(result);
                m.error_position = i;
                asprintf(&m.result, "Invalid XML entity in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
        }
        else if (sgml_subset == SGML_SUBSET_XML && !strncmp(&input[i], "]]>", sizeof "]]>" - 1)) {
            in_cdata = false;
            i += sizeof "]]>" - 1;
            continue;
        }
        result[result_length++] = input[i++];
    }
    result[result_length++] = '\0';
    m.result = result;
    return m;
}

static char *xml_encode(const char *input, const int input_length, int *output_length)
{
    int added_length_with_cdata = sizeof "<![CDATA[]]>" - 1;
    int added_length_with_entities = 0;
    int i = 0;
    while (true) {
        if (input[i] == '\0') {
            break;
        }
        else if (input[i] == '<') {
            added_length_with_entities += sizeof "&lt;" - 2;
        }
        else if (input[i] == '>') {
            added_length_with_entities += sizeof "&gt;" - 2;
        }
        else if (input[i] == '&') {
            added_length_with_entities += sizeof "&amp;" - 2;
        }
        else if (input[i] == ']' && input[i + 1] == ']' && input[i + 2] == '>') {
            added_length_with_cdata += sizeof "]]><![CDATA[" - 1;
        }
        i += 1;
    }
    if (added_length_with_entities == 0) {
        char *output = malloc(input_length + 1);
        memcpy(output, input, input_length);
        output[input_length] = '\0';
        *output_length = input_length;
        return output;
    }
    if (added_length_with_cdata < added_length_with_entities + 1) {
        char *output = malloc(input_length + added_length_with_cdata + 1);
        if (output == NULL) {
            return NULL;
        }
        strcpy(output, "<![CDATA[");
        *output_length = sizeof "<![CDATA[" - 1;
        for (i = 0; input[i] != '\0'; ++i) {
            if (!strncmp(&input[i], "]]>", sizeof "]]>" - 1)) {
                strcpy(&output[*output_length], "]]]]><![CDATA[>");
                *output_length = sizeof "]]]]><![CDATA[>" - 1;
                i += 2;
            }
            else {
                output[*output_length] = input[i];
                *output_length += 1;
            }
        }
        strcpy(&output[*output_length], "]]>");
        *output_length = sizeof "]]>" - 1;
        return output;
    }
    else {
        char *output = malloc(input_length + added_length_with_entities + 1);
        if (output == NULL) {
            return NULL;
        }
        *output_length = 0;
        for (i = 0; input[i] != '\0'; ++i) {
            if (input[i] == '<') {
                strcpy(&output[*output_length], "&lt;");
                *output_length += sizeof "&lt;" - 1;
            }
            else if (input[i] == '>') {
                strcpy(&output[*output_length], "&gt;");
                *output_length += sizeof "&gt;" - 1;
            }
            else if (input[i] == '&') {
                strcpy(&output[*output_length], "&amp;");
                *output_length += sizeof "&amp;" - 1;
            }
            else {
                output[*output_length] = input[i];
                *output_length += 1;
            }
        }
        output[*output_length] = '\0';
        return output;
    }
}

static struct Minification minify_sgml(const char *sgml, enum SGMLSubset sgml_subset)
{
    int input_strlen = strlen(sgml);
    struct Minification m = {0};
    char *sgml_min = malloc(input_strlen + 1);
    if (sgml_min == NULL) {
        m.result = strdup(strerror(errno));
        m.result_is_error_message = true;
        return m;
    }
    int sgml_min_length = 0;

    enum {
        SYNTAX_BLOCK_TAG,
        SYNTAX_BLOCK_CONTENT,
        SYNTAX_BLOCK_DOCTYPE,
    } syntax_block = SYNTAX_BLOCK_CONTENT;

    enum {
        SCRIPT_TYPE_JAVASCRIPT,
        SCRIPT_TYPE_JSON,
        SCRIPT_TYPE_OTHER
    } script_type;

    int i = 0;
    const char *current_tag;
    int current_tag_length = 0;
    bool is_closing_tag;
    bool whitespace_before_tag;
    int (*tagncmp)(const char *, const char *, size_t);
    tagncmp = (sgml_subset == SGML_SUBSET_XML ? strncmp : strnicmp);
    const char *value, *attribute;
    int value_length, attribute_length;

    while (true) {
        if (sgml[i] == '\0') {
            if (syntax_block == SYNTAX_BLOCK_TAG) {
                free(sgml_min);
                m.error_position = i;
                asprintf(&m.result, "Unexpected end of document expected `>` after line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            sgml_min[sgml_min_length] = '\0';
            break;
        }

        // Next we try to minify inline styles and scripts. We pass the current line and column to the
        // minification callback to make its error messages more precise. But what we can't correct
        // easily are offsets in line or column caused by XML entities.
        //
        // To solve this problem we would need to return the errorneous line & column and calculate
        // correct the offset using an extra function:
        // `correct_offset_created_by_xml_entities(&line, &column, undecoded_tag_content)`

        const char *tag_content_delimiter = NULL;
        int tag_content_delimiter_length;
        struct Minification (*tag_content_minify_callback)(const char *) = NULL;

        if (syntax_block == SYNTAX_BLOCK_CONTENT &&
            current_tag_length == sizeof "script" - 1 &&
            !tagncmp(current_tag, "script", sizeof "script" - 1))
        {
            tag_content_delimiter = "</script";
            tag_content_delimiter_length = sizeof "</script" - 1;
            if (script_type == SCRIPT_TYPE_JAVASCRIPT) {
                tag_content_minify_callback = minify_js;
            }
            else if (script_type == SCRIPT_TYPE_JSON) {
                tag_content_minify_callback = minify_json;
            }
            else if (script_type == SCRIPT_TYPE_OTHER) {
                tag_content_minify_callback = NULL;
            }
        }
        if (syntax_block == SYNTAX_BLOCK_CONTENT &&
            current_tag_length == sizeof "style" - 1 &&
            !tagncmp(current_tag, "style", sizeof "style" - 1))
        {
            tag_content_delimiter = "</style";
            tag_content_delimiter_length = sizeof "</style" - 1;
            tag_content_minify_callback = minify_css;
        }
        if (tag_content_delimiter != NULL) {
            int content_start_i = i;
            bool in_cdata = false;
            while (true) {
                if (sgml[i] == '\0') {
                    free(sgml_min);
                    m.error_position = i;
                    asprintf(&m.result,
                        "Unexpected end of document, expected `%s>` after line %%d, column %%d\n",
                        tag_content_delimiter);
                    m.result_is_error_message = true;
                    return m;
                }
                if (sgml_subset == SGML_SUBSET_XML &&
                    !strncmp(&sgml[i], "<![CDATA[", sizeof "<![CDATA[" - 1))
                {
                    in_cdata = true;
                    i += sizeof "<![CDATA[" - 1;
                    continue;
                }
                if (sgml_subset == SGML_SUBSET_XML &&
                    !strncmp(&sgml[i], "]]>", sizeof "]]>" - 1))
                {
                    in_cdata = false;
                    i += sizeof "]]>" - 1;
                    continue;
                }
                if (!in_cdata &&
                    !tagncmp(&sgml[i], tag_content_delimiter, sizeof tag_content_delimiter - 1))
                {
                    current_tag_length = 0;
                    break;
                }
                i += 1;
            }
            if (tag_content_minify_callback == NULL) {
                memcpy(&sgml_min[sgml_min_length], &sgml[content_start_i], i - content_start_i);
                sgml_min_length += i - content_start_i;
                continue;
            }
            if (sgml_subset == SGML_SUBSET_XML) {
                struct Minification decoded = sgml_decode(&sgml[content_start_i], i - content_start_i,
                    SGML_SUBSET_XML);
                if (decoded.result == NULL) {
                    free(sgml_min);
                    decoded.error_position += content_start_i;
                    return decoded;
                }
                m = tag_content_minify_callback(decoded.result);
                if (m.result == NULL) {
                    free(sgml_min);
                    sgml_correct_error_position(&sgml[content_start_i], decoded.result, &m.error_position,
                        sgml_subset);
                    m.error_position += content_start_i;
                    m.result_is_error_message = true;
                    return m;
                }
                int new_tag_content_length;
                char *new_tag_content = xml_encode(m.result, i - content_start_i, &new_tag_content_length);
                free(m.result);
                if (new_tag_content == NULL) {
                    free(sgml_min);
                    m.result = strdup(strerror(errno));
                    m.result_is_error_message = true;
                    return m;
                }
                m.result = new_tag_content;
                if (new_tag_content_length > i - content_start_i) {
                    char *new_sgml_min = realloc(
                        sgml_min,
                        input_strlen + new_tag_content_length - i + content_start_i
                    );
                    if (new_sgml_min == NULL) {
                        free(sgml_min);
                        m.result = strdup(strerror(errno));
                        m.result_is_error_message = true;
                        return m;
                    }
                    sgml_min = new_sgml_min;
                }
            }
            else {
                char *tag_content = malloc(i - content_start_i + 1);
                if (tag_content == NULL) {
                    free(sgml_min);
                    m.result = strdup(strerror(errno));
                    m.result_is_error_message = true;
                    return m;
                }
                memcpy(tag_content, &sgml[content_start_i], i - content_start_i);
                tag_content[i - content_start_i] = '\0';
                m = tag_content_minify_callback(tag_content);
                free(tag_content);
                if (m.result == NULL) {
                    free(sgml_min);
                    m.error_position += content_start_i;
                    return m;
                }
            }
            sgml_min_length += stpcpy(&sgml_min[sgml_min_length], m.result) -
                               &sgml_min[sgml_min_length];
            free(m.result);
            continue;
        }
        if (!strncmp(&sgml[i], "<!--", 4)) {
            int comment_start_i = i;
            i += 4;
            while (sgml[i] != '\0' && strncmp(&sgml[i], "-->", 3)) {
                i += 1;
            }
            if (sgml[i] == '\0') {
                free(sgml_min);
                m.error_position = comment_start_i;
                asprintf(&m.result, "Unclosed comment starting in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
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
        if (sgml_subset == SGML_SUBSET_XML && !strncmp(&sgml[i], "<![CDATA[", sizeof "<![CDATA[" - 1)) {
            memcpy(&sgml_min[sgml_min_length], "<![CDATA[", sizeof "<![CDATA[" - 1);
            sgml_min_length += sizeof "<![CDATA[" - 1;
            i += sizeof "<![CDATA[" - 1;
            while (true) {
                if (!strncmp(&sgml[i], "]]>", sizeof "]]>" - 1)) {
                    memcpy(&sgml_min[sgml_min_length], "]]>", sizeof "]]>" - 1);
                    sgml_min_length += sizeof "]]>" - 1;
                    i += sizeof "]]>" - 1;
                    break;
                }
                sgml_min[sgml_min_length++] = sgml[i];
                i += 1;
            }
            continue;
        }
        if (sgml[i] == '<') {
            // Consume `<` and tag name
            if (syntax_block == SYNTAX_BLOCK_TAG) {
                free(sgml_min);
                m.error_position = i;
                asprintf(&m.result, "Illegal `<` in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            whitespace_before_tag = sgml_min_length > 0 && is_whitespace(sgml_min[sgml_min_length - 1]);
            sgml_min[sgml_min_length++] = '<';
            i += 1;
            if (!strnicmp(&sgml[i], "!DOCTYPE", sizeof "!DOCTYPE" - 1)) {
                syntax_block = SYNTAX_BLOCK_DOCTYPE;
                continue;
            }

            current_tag = &sgml[i];
            is_closing_tag = (sgml[i] == '/');
            if (is_closing_tag) {
                sgml_min[sgml_min_length++] = '/';
                i += 1;
            }
            if (!(
                sgml[i] >= 'a' && sgml[i] <= 'z' ||
                sgml[i] >= 'A' && sgml[i] <= 'Z' ||
                sgml[i] == ':' || sgml[i] == '_' ||
                sgml[i] == '?'
            )) {
                free(sgml_min);
                m.error_position = i - 1;
                asprintf(&m.result,
                    "`%c` in line %%d, column %%d is followed by an illegal character\n", sgml[i - 1]);
                m.result_is_error_message = true;
                return m;
            }
            sgml_min[sgml_min_length++] = sgml[i];
            current_tag_length = 1;
            while (
                sgml[i + current_tag_length] >= 'a' && sgml[i + current_tag_length] <= 'z' ||
                sgml[i + current_tag_length] >= 'A' && sgml[i + current_tag_length] <= 'Z' ||
                sgml[i + current_tag_length] >= '0' && sgml[i + current_tag_length] <= '9' ||
                sgml[i + current_tag_length] == '-'
            ) {
                current_tag_length += 1;
                sgml_min[sgml_min_length++] = sgml[i + current_tag_length - 1];
            }
            if (
                sgml[i + current_tag_length] != '/' && sgml[i + current_tag_length] != '>' &&
                !is_whitespace(sgml[i + current_tag_length])
            ) {
                free(sgml_min);
                m.error_position = i + current_tag_length;
                asprintf(&m.result, "Illegal character in tag name in in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            syntax_block = SYNTAX_BLOCK_TAG;
            script_type == SCRIPT_TYPE_JAVASCRIPT;
            attribute_length = 0;
            i += current_tag_length;
            continue;
        }
        if (sgml[i] == '>') {
            if (syntax_block == SYNTAX_BLOCK_CONTENT) {
                free(sgml_min);
                m.error_position = i;
                asprintf(&m.result, "Illegal `>` in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            if (sgml[i - 1] == '/' && sgml_min[sgml_min_length - 1] != '=') {
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
                current_tag_length = 0;
                i += 1;
                continue;
            }

            i += 1;
            syntax_block = SYNTAX_BLOCK_CONTENT;

            if (sgml_subset == SGML_SUBSET_XML) {
                // Ignore whitespace except between opening and closing tags

                int k = i;
                while (is_whitespace(sgml[k])) {
                    k += 1;
                }
                if (sgml[k] == '<' && (is_closing_tag || sgml[k + 1] != '/')) {
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
        if (syntax_block == SYNTAX_BLOCK_TAG && is_whitespace(sgml[i])) {
            while (is_whitespace(sgml[i])) {
                i += 1;
            }
            if (sgml[i] != '=' && sgml_min[sgml_min_length - 1] != '=' && sgml[i] != '>' && sgml[i] != '/') {
                sgml_min[sgml_min_length++] = ' ';
            }
            if (is_closing_tag && sgml[i] != '>') {
                free(sgml_min);
                m.error_position = i;
                asprintf(&m.result, "Illegal content in line %%d, column %%d after whitespace in closing tag\n");
                m.result_is_error_message = true;
                return m;
            }
            continue;
        }
        if (syntax_block == SYNTAX_BLOCK_TAG && sgml[i] != '=') {
            // Consume attribute
            if (sgml[i] == '"' || sgml[i] == '\'') {
                free(sgml_min);
                m.error_position = i;
                asprintf(&m.result, "Illegal character `%c` in line %%d, column %%d\n", sgml[i]);
                m.result_is_error_message = true;
                return m;
            }
            attribute = &sgml[i];
            attribute_length = 0;
            while (strchr("\"' \t\r\n<>=/", sgml[i]) == NULL) {
                sgml_min[sgml_min_length++] = sgml[i];
                attribute_length += 1;
                i += 1;
            }
            if (sgml[i] == '/' && sgml[i + 1] != '>') {
                free(sgml_min);
                m.error_position = i;
                asprintf(&m.result, "`/` in line %%d, column %%d is not followed by `>` \n");
                m.result_is_error_message = true;
                return m;
            }
            if (sgml[i] == '/' &&sgml[i + 1] == '>') {
                if (sgml_subset == SGML_SUBSET_XML) {
                    sgml_min[sgml_min_length++] = '/';
                }
                i += 1;
                continue;
            }
            if (strchr("=> \r\t\n/", sgml[i]) == NULL) {
                free(sgml_min);
                m.error_position = i;
                asprintf(&m.result, "Illegal character `%c` after attribute in line %%d, column %%d\n",
                    sgml[i]);
                m.result_is_error_message = true;
                return m;
            }
            continue;
        }
        if (syntax_block == SYNTAX_BLOCK_TAG && sgml[i] == '=') {
            // Consume `=` followed by quoted or unquoted value
            if (attribute_length == 0) {
                free(sgml_min);
                m.error_position = i;
                asprintf(&m.result, "No attribute before `=` in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }

            i += 1;
            while (is_whitespace(sgml[i])) {
                i += 1;
            }
            if (sgml[i] == '=' || sgml[i] == '>') {
                free(sgml_min);
                m.error_position = i;
                asprintf(&m.result, "No value after `=` in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }
            if (sgml_subset == SGML_SUBSET_XML && sgml[i] != '"' && sgml[i] != '\'') {
                free(sgml_min);
                m.error_position = i;
                asprintf(&m.result, "XML requires a quote after `=` in line %%d, column %%d\n");
                m.result_is_error_message = true;
                return m;
            }

            sgml_min[sgml_min_length++] = '=';
            if (sgml[i] == '"' || sgml[i] == '\'') {
                char quote = sgml[i];
                int string_start_i = i;
                i += 1;
                value = &sgml[i];
                value_length = 0;
                bool need_quotes;
                if (sgml_subset == SGML_SUBSET_XML || syntax_block == SYNTAX_BLOCK_DOCTYPE) {
                    need_quotes = true;
                }
                else if (sgml[i] == quote) {
                    need_quotes = true;
                }
                else {
                    need_quotes = false;
                    int k = i;
                    while (sgml[k] != quote) {
                        if (strchr(" \r\t\n=\"'/", sgml[k]) != NULL) {
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
                    value_length += 1;
                    i += 1;
                }
                if (sgml[i] == '\0') {
                    free(sgml_min);
                    m.error_position = string_start_i;
                    asprintf(&m.result, "Unclosed string starting in line %%d, column %%d\n");
                    m.result_is_error_message = true;
                    return m;
                }
                i += 1;
                if (
                    !is_whitespace(sgml[i]) && sgml[i] != '>' &&
                    (sgml[i] != '/' || sgml[i + 1] != '>') &&
                    (sgml[i] != '?' || sgml[i + 1] != '>')
                ) {
                    free(sgml_min);
                    m.error_position = i;
                    asprintf(&m.result, "Illegal character after `%c` in line %%d, column %%d\n", quote);
                    m.result_is_error_message = true;
                    return m;
                }
                if (need_quotes) {
                    sgml_min[sgml_min_length++] = quote;
                }
            }
            else {
                value = &sgml[i];
                value_length = 0;
                while (strchr(" \r\t\n >=\"'", sgml[i]) == NULL) {
                    sgml_min[sgml_min_length++] = sgml[i];
                    i += 1;
                    value_length += 1;
                }
            }

            // Checking script type

            struct Minification decoded_value = sgml_decode(value, value_length, SGML_SUBSET_HTML);
            if (decoded_value.result == NULL) {
                free(sgml_min);
                decoded_value.error_position += i;
                return decoded_value;
            }
            if (current_tag_length == sizeof "script" - 1 &&
                !tagncmp(current_tag, "script", sizeof "script" - 1) &&
                attribute_length == sizeof "type" - 1 && !tagncmp(attribute, "type", sizeof "type" - 1))
            {
                if (!strcmp(decoded_value.result, "application/json+ld")) {
                    script_type = SCRIPT_TYPE_JSON;
                }
                else if (!strcmp(decoded_value.result, "importmap")) {
                    script_type = SCRIPT_TYPE_JSON;
                }
                else if (!strcmp(decoded_value.result, "module")) {
                    script_type = SCRIPT_TYPE_JAVASCRIPT;
                }
                else if (!strcmp(decoded_value.result, "text/javascript")) {
                    script_type = SCRIPT_TYPE_JAVASCRIPT;
                }
                else {
                    script_type = SCRIPT_TYPE_OTHER;
                }
            }
            free(decoded_value.result);
            continue;
        }
        if (sgml_subset == SGML_SUBSET_HTML &&
            syntax_block == SYNTAX_BLOCK_CONTENT && is_whitespace(sgml[i]))
        {
            if (current_tag_length == sizeof "pre" - 1 &&
                !strnicmp(current_tag, "pre", sizeof "pre" - 1))
            {
                sgml_min[sgml_min_length++] = sgml[i];
                i += 1;
                continue;
            }
            while (is_whitespace(sgml[i])) {
                i += 1;
            }
            if (whitespace_before_tag && sgml_min[sgml_min_length - 1] == '>') {
                continue;
            }
            sgml_min[sgml_min_length++] = ' ';
            continue;
        }
        sgml_min[sgml_min_length++] = sgml[i];
        i += 1;
    }
    m.result = realloc(sgml_min, sgml_min_length + 1);
    if (m.result == NULL) {
        free(sgml_min);
        m.result = strdup(strerror(errno));
        m.result_is_error_message = true;
    }
    return m;
}

struct Minification minify_xml(const char *xml)
{
    return minify_sgml(xml, SGML_SUBSET_XML);
}

struct Minification minify_html(const char *html)
{
    return minify_sgml(html, SGML_SUBSET_HTML);
}

struct LineColumn
{
    int line;
    int column;
};

static struct LineColumn position_to_line_column(const char *text, int position)
{
    struct LineColumn lc = {.line = 1, .column = 0};
    for (int i = 0; i <= position; ++i) {
        lc.line += (text[i] == '\n');
        lc.column = (text[i] != '\n') * (lc.column + 1);
    }
    return lc;
}

int main(int argc, const char *argv[])
{
    bool benchmark = false;
    bool print_usage = false;
    const char *format_str = NULL;
    const char *input_filename = NULL;
    enum {FORMAT_JS, FORMAT_CSS, FORMAT_XML, FORMAT_HTML, FORMAT_JSON} format;

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
            print_usage = true;
            break;
        }
    }
    if (format_str == NULL || input_filename == NULL) {
        print_usage = true;
    }
    else if (!strcmp(format_str, "js")) {
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
        print_usage = true;
    }

    if (print_usage) {
        fputs("Usage: minify <css|js|xml|html|json> <input file|-> [--benchmark]\n", stderr);
        return EXIT_FAILURE;
    }

    char *input = file_get_content(input_filename);
    if (input == NULL) {
        perror(input_filename);
        return EXIT_FAILURE;
    }
    struct Minification m;
    switch (format) {
    case FORMAT_JS:
        m = minify_js(input);
        break;
    case FORMAT_CSS:
        m = minify_css(input);
        break;
    case FORMAT_XML:
        m = minify_xml(input);
        break;
    case FORMAT_HTML:
        m = minify_html(input);
        break;
    case FORMAT_JSON:
    default:
        m = minify_json(input);
        break;
    }
    if (m.result_is_error_message) {
        struct LineColumn line_column = position_to_line_column(input, m.error_position);
        free(input);
        fprintf(stderr, m.result, line_column.line, line_column.column);
        free(m.result);
        return EXIT_FAILURE;
    }
    if (benchmark) {
        int strlen_input = strlen(input);
        int strlen_minification = strlen(m.result);
        printf(
            "Reduced the size by %.1f%% from %d to %d bytes\n",
            100.0 - 100.0 * strlen_minification / strlen_input, strlen_input, strlen_minification
        );
    }
    else {
        fputs(m.result, stdout);
    }
    free(m.result);
    free(input);
    return EXIT_SUCCESS;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <ctype.h>

static char *file_get_contents(char *filename)
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

static bool is_whitespace(char const c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int css_skip_whitespaces_comments(char const *css, int i, int *line, int *last_newline)
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

static bool is_nestable_atrule(char const *atrule, int atrule_length)
{
    return
        sizeof "@media" - 1 == atrule_length &&
        ! strncmp(atrule, "@media", atrule_length) ||

        sizeof "@layer " - 1 == atrule_length &&
        ! strncmp(atrule, "@layer", atrule_length) ||

        sizeof "@container" - 1 == atrule_length &&
        ! strncmp(atrule, "@container", atrule_length) ||

        sizeof "@keyframes" - 1 == atrule_length &&
        ! strncmp(atrule, "@keyframes", atrule_length);
}

char *css_minify(char const *css)
{
    // Returns the minified CSS, which has to be freed, or NULL in case of error.

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
    int css_min_length = 0;

    char const *atrule = NULL;
    int atrule_length;

    int line = 1, last_newline = -1;
    int i = css_skip_whitespaces_comments(css, 0, &line, &last_newline);
    int nesting_level = 0;

    while (true) {
        if (css[i] == '\0') {
            if (syntax_block != SYNTAX_BLOCK_RULE_START) {
                if (syntax_block == SYNTAX_BLOCK_STYLE) {
                    fprintf(stderr, "Unexpected end of file, expected }\n");
                }
                else if (syntax_block == SYNTAX_BLOCK_QRULE) {
                    fprintf(stderr, "Unexpected end of file, expected {…}\n");
                }
                else if (syntax_block == SYNTAX_BLOCK_ATRULE) {
                    fprintf(stderr, "Unexpected end of file, expected ; or {…}\n");
                }
                free(css_min);
                return NULL;
            }
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
                    fprintf(stderr, "Unexpected } in line %d, column %d\n", line, i - last_newline);
                    free(css_min);
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
                fprintf(stderr, "Unexpected %c in line %d, column %d\n",
                        css[i], line, i - last_newline);
                free(css_min);
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
                    fprintf(stderr, "Expected ) in line %d, column %d\n",
                            line, i - last_newline);
                    free(css_min);
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
                    if (css[i] == '\0') {
                        fprintf(stderr, "Unexpected end of file, expected )\n");
                    }
                    else if (is_whitespace(css[i - 1])) {
                        fprintf(stderr, "Illegal white-space in URL in line %d, column %d\n",
                                line, i - last_newline - 1);
                    }
                    free(css_min);
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
                fprintf(stderr, "Unexpected { in line %d, column %d\n", line, i - last_newline);
                free(css_min);
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
                if (
                    // Remove whitespace AFTER these characters:
                    css[i - 1] == '(' ||
                    css[i - 1] == ',' ||
                    css[i - 1] == '<' ||
                    css[i - 1] == '>' ||
                    css[i - 1] == ':'
                ) {
                    i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                }
                else {
                    i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                    if (! (
                        // Remove whitespace BEFORE these characters:
                        css[i] == ')' ||
                        css[i] == ',' ||
                        css[i] == '<' ||
                        css[i] == '>' ||
                        css[i] == ':'
                    )) {
                        css_min[css_min_length++] = ' ';
                    }
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_ATRULE_SQUARE_BRACKETS ||
                     syntax_block == SYNTAX_BLOCK_QRULE_SQUARE_BRACKETS)
            {
                if (
                    // Remove whitespace AFTER these characters:
                    css[i - 1] == '[' ||
                    css[i - 1] == '=' ||
                    css[i - 1] == ','
                ) {
                    i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                }
                else {
                    i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                    if (! (
                        // Remove whitespace BEFORE these characters:
                        css[i] == ']' ||
                        css[i] == '=' ||
                        css[i] == ',' ||
                        css[i] == '*' && css[i] == '=' ||
                        css[i] == '$' && css[i] == '=' ||
                        css[i] == '^' && css[i] == '=' ||
                        css[i] == '~' && css[i] == '=' ||
                        css[i] == '|' && css[i] == '='
                    )) {
                        css_min[css_min_length++] = ' ';
                    }
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_ATRULE) {
                if (
                    // Remove whitespace AFTER these characters:
                    css[i - 1] == ',' ||
                    css[i - 1] == ')' ||
                    css[i - 1] == '('
                ) {
                    i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                }
                else {
                    // Remove white-space before ( in `@media (...){}` but not in
                    // `@media all and (...){}`.

                    int before_whitespace = i - 1;
                    i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                    bool round_bracket_preceded_by_atrule =
                        css[i] == '(' &&
                        &atrule[atrule_length - 1] == &css[before_whitespace];
                    if (! (
                        // Remove whitespace BEFORE these characters:
                        round_bracket_preceded_by_atrule ||
                        css[i] == ',' ||
                        css[i] == ')' ||
                        css[i] == ';' ||
                        css[i] == '{'
                    )) {
                        css_min[css_min_length++] = ' ';
                    }
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_QRULE) {
                if (
                    // Remove whitespace AFTER these characters:
                    css[i - 1] == '~' ||
                    css[i - 1] == '>' ||
                    css[i - 1] == '+' ||
                    css[i - 1] == ',' ||
                    css[i - 1] == ']'
                ) {
                    i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                }
                else {
                    i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                    if (! (
                        // Remove whitespace BEFORE these characters:
                        css[i] == '~' ||
                        css[i] == '>' ||
                        css[i] == '+' ||
                        css[i] == ',' ||
                        css[i] == '[' ||
                        css[i] == '{'
                    )) {
                        css_min[css_min_length++] = ' ';
                    }
                }
            }
            else if (syntax_block == SYNTAX_BLOCK_STYLE) {
                if (
                    // Remove whitespace AFTER these characters:
                    css[i - 1] == '{' ||
                    css[i - 1] == ':' ||
                    css[i - 1] == ','
                ) {
                    i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                }
                else {
                    i = css_skip_whitespaces_comments(css, i, &line, &last_newline);
                    if (! (
                        // Remove whitespace BEFORE these characters:
                        css[i] == '}' ||
                        css[i] == ':' ||
                        css[i] == ',' ||
                        css[i] == ';' ||
                        css[i] == '!'
                    )) {
                        css_min[css_min_length++] = ' ';
                    }
                }
            }
            continue;
        }
        css_min[css_min_length++] = css[i];
        i += 1;
    }
    css_min[css_min_length++] = '\0';
    return css_min;
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        fputs("Specify the input file or - to read from standard input.\n", stderr);
        return EXIT_FAILURE;
    }
    char *css = file_get_contents(argv[1]);
    if (css == NULL) {
        perror(NULL);
        return EXIT_FAILURE;
    }
    char *css_min = css_minify(css);
    free(css);
    if (css_min == NULL) {
        return EXIT_FAILURE;
    }
    puts(css_min);
    free(css_min);
    return EXIT_SUCCESS;
}

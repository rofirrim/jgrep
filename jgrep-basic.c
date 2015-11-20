#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

static int matchstar(int c, const char *regexp, const char *text);

/* matchhere: search for regexp at beginning of text */
static int matchhere(const char *regexp, const char *text)
{
    if (regexp[0] == '\0')
        return 1;
    if (regexp[1] == '*')
        return matchstar(regexp[0], regexp+2, text);
    if (regexp[0] == '$' && regexp[1] == '\0')
        return *text == '\0';
    if (*text!='\0' && (regexp[0]=='.' || regexp[0]==*text))
        return matchhere(regexp+1, text+1);
    return 0;
}

/* matchstar: search for c*regexp at beginning of text */
static int matchstar(int c, const char *regexp, const char *text)
{
    do {    /* a * matches zero or more instances */
        if (matchhere(regexp, text))
            return 1;
    } while (*text != '\0' && (*text++ == c || c == '.'));
    return 0;

}

/* match: search for regexp anywhere in text */
static int match(const char *regexp, const char *text)
{
    if (regexp[0] == '^')
        return matchhere(regexp+1, text);
    do {    /* must look even if string is empty */
        if (matchhere(regexp, text))
            return 1;
    } while (*text++ != '\0');
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s regex filename\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char* regexp = argv[1];

    FILE *f = fopen(argv[2], "r");
    if (f == NULL)
    {
        fprintf(stderr, "error opening file '%s': %s\n",
                argv[2],
                strerror(errno));
        exit(EXIT_FAILURE);
    }

    char* line = NULL;
    size_t length = 0;

    while (getline(&line, &length, f) != -1)
    {
      if (match(regexp, line))
        fprintf(stdout, "%s", line);
    }

    free(line);
    fclose(f);

    return 0;
}

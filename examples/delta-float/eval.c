// Find the line with the maximal y.

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

static inline bool is_prefix (const char *pre, const char *s)
{
    return 0 == strncmp (pre, s, strlen (pre));
}

typedef struct
{
    unsigned xx;
    int n, num;
    float x, y;
} max_t;

int main (int argc, char *argv[])
{
    max_t m = { 0, 0, 0, nanf(""), 0.0 };

    bool out_ulp = argc > 1 && !strcasecmp (argv[1], "-out=ulp");

    for (ssize_t n_read;;)
    {
        char *line = NULL;
        size_t n_chars = 0;
        n_read = getline (&line, &n_chars, stdin);
        if (n_read == -1)
            break;

        if (is_prefix ("== ", line))
        {
            size_t len = strlen (line);
            const char *nl = len && line[len - 1] == '\n' ? "" : "\n";

            max_t l;

            int n_scans = sscanf (line, "== %d/%d: %i: %f -> %f",
                                  &l.n, &l.num, &l.xx, &l.x, &l.y);

            if (n_scans != 5)
                printf ("not scanned: %s%s\n", line, nl);
            else
            {
                if (!isnanf (l.x)
                    && (l.y > m.y || isnanf (m.x)))
                    m = l;
            }
        }
        free (line);
    }

    printf ("eval: %d/%d: 0x%08x = %e -> ", m.n, m.num, m.xx, m.x);
    if (out_ulp)
        printf ("%.1f ulp", m.y);
    else
    {
        printf ("%e", m.y);
        if (m.y > 0 && !isnan (m.y))
        {
            printf ("  log10: %f", log10 (m.y));
            printf ("  log2: %f", log2 (m.y));
        }
    }
    printf ("\n");
    
    return EXIT_SUCCESS;
}


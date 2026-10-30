/*
 * test_bindings -- the input config the launcher writes, read back.
 *
 * The thing that matters here is not that the launcher can save a file.
 * It is that what it saves is what the runtime looks for: a control name
 * or a source spelling the runtime does not know is a binding that
 * silently does nothing, which is the worst way for this to fail.
 *
 * So this checks the vocabulary against the runtime's own source, not
 * against a copy of it, and then checks that a file survives the trip.
 */
#include "launcher_bindings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

static void fail(const char *what)
{
    printf("  FAIL: %s\n", what);
    failures++;
}

static void check(int ok, const char *what)
{
    if (!ok) fail(what);
}

static void check_str(const char *got, const char *want, const char *what)
{
    if (!got || !want || strcmp(got, want) != 0) {
        printf("  FAIL: %s (got %s, want %s)\n", what,
               got ? got : "(null)", want ? want : "(null)");
        failures++;
    }
}

/* The runtime's own file, read as text. Anything the launcher can write
 * has to appear in it. */
static char *slurp(const char *path)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *buf;

    if (!f)
        return NULL;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = (char *)malloc((size_t)n + 1);
    if (!buf) { fclose(f); return NULL; }
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(buf); return NULL; }
    fclose(f);
    buf[n] = '\0';
    return buf;
}

/* Is `needle` present as a quoted string in the runtime's source? */
static int mentions(const char *hay, const char *needle)
{
    char quoted[64];

    snprintf(quoted, sizeof quoted, "\"%s\"", needle);
    return strstr(hay, quoted) != NULL;
}

int main(int argc, char **argv)
{
    const char *runtime_src = (argc > 1) ? argv[1] : NULL;
    const char *tmp = "test_bindings_tmp.json";
    BindConfig a, b;
    int i;

    printf("test_bindings\n");

    /* --- the vocabulary, against the runtime rather than a copy ------- */
    if (runtime_src) {
        char *src = slurp(runtime_src);

        if (!src) {
            fail("cannot read the runtime's input_bindings.c");
        } else {
            for (i = 0; i < BIND_CONTROLS; i++)
                if (!mentions(src, bind_control_names[i])) {
                    printf("  FAIL: the runtime does not know control \"%s\"\n",
                           bind_control_names[i]);
                    failures++;
                }

            /* Every source the defaults use must be spelled the way the
             * runtime spells it. Pad buttons and key names appear in its
             * tables; the half-axes are built from characters, so those
             * are checked by shape instead. */
            bind_defaults(&a);
            for (i = 0; i < BIND_CONTROLS; i++) {
                const char *p = a.port[0].pad_src[i];
                const char *k = a.port[0].key_src[i];

                if (p && *p) {
                    const char *name = p + 4;          /* past "pad:" */
                    size_t len = strlen(name);
                    int axis = (len == 3 &&
                                (name[0] == 'l' || name[0] == 'r') &&
                                (name[1] == 'x' || name[1] == 'y') &&
                                (name[2] == '+' || name[2] == '-'));
                    int trig = (strcmp(name, "lt") == 0 || strcmp(name, "rt") == 0);

                    if (!axis && !trig && !mentions(src, name)) {
                        printf("  FAIL: the runtime does not know pad source \"%s\"\n", p);
                        failures++;
                    }
                }
                if (k && *k && strlen(k) > 5 && !mentions(src, k + 4)) {
                    printf("  FAIL: the runtime does not know key name \"%s\"\n", k);
                    failures++;
                }
            }
            free(src);
        }
    } else {
        printf("  (runtime source not given; vocabulary check skipped)\n");
    }

    /* --- the round trip ---------------------------------------------- */
    bind_defaults(&a);
    check_str(a.port[0].pad_src[0], "pad:a", "default: A is pad A");
    check_str(a.port[0].key_src[0], "key:Z", "default: A is also Z");
    check_str(a.port[1].key_src[0], "", "default: the keyboard is port 1 only");
    check(a.port[2].pad == 2, "default: port 3 takes pad 3");

    /* Change something on each port, including a stick half and an unbind. */
    snprintf(a.port[0].pad_src[0], BIND_SOURCE_LEN, "pad:rshoulder");
    snprintf(a.port[0].key_src[0], BIND_SOURCE_LEN, "key:SPACE");
    snprintf(a.port[1].pad_src[14], BIND_SOURCE_LEN, "pad:ry+");
    a.port[2].device = BIND_DEV_KEYBOARD;
    a.port[2].pad = -1;
    a.port[3].device = BIND_DEV_NONE;
    a.port[0].pad_src[5][0] = '\0';
    a.port[0].key_src[5][0] = '\0';
    a.port[0].deadzone = 9000;

    check(bind_save(tmp, &a) != 0, "saved");
    check(bind_load(tmp, &b) != 0, "loaded");

    check_str(b.port[0].pad_src[0], "pad:rshoulder", "round trip: rebound pad source");
    check_str(b.port[0].key_src[0], "key:SPACE", "round trip: rebound key source");
    check_str(b.port[1].pad_src[14], "pad:ry+", "round trip: a stick half");
    check(b.port[2].device == BIND_DEV_KEYBOARD, "round trip: keyboard device");
    check(b.port[3].device == BIND_DEV_NONE, "round trip: no device");
    check(b.port[0].deadzone == 9000, "round trip: deadzone");
    check_str(b.port[0].pad_src[5], "", "round trip: an unbound control stays unbound");
    check_str(b.port[0].key_src[5], "", "round trip: unbound on the keyboard too");
    check_str(b.port[0].pad_src[1], "pad:b", "round trip: an untouched control");

    /* A file with keys this does not model must still read. */
    {
        FILE *f = fopen(tmp, "wb");

        if (f) {
            fputs("{\n"
                  "  \"version\": 1,\n"
                  "  \"something_new\": {\"nested\": [1, 2, {\"deep\": true}]},\n"
                  "  \"controllers\": [\n"
                  "    { \"port\": 1, \"device\": \"xinput:2\", \"deadzone\": 1234,\n"
                  "      \"unknown\": \"ignored\",\n"
                  "      \"bindings\": { \"a\": [\"pad:y\", \"key:K\"],\n"
                  "                      \"not_a_control\": [\"pad:a\"] } }\n"
                  "  ]\n"
                  "}\n", f);
            fclose(f);
            check(bind_load(tmp, &b) != 0, "a file with unknown keys still reads");
            check_str(b.port[0].pad_src[0], "pad:y", "unknown keys stepped over");
            check_str(b.port[0].key_src[0], "key:K", "unknown keys stepped over (key)");
            check(b.port[0].pad == 2, "device honoured past an unknown key");
            check(b.port[0].deadzone == 1234, "deadzone honoured");
        }
    }

    remove(tmp);
    if (failures)
        printf("test_bindings: %d FAILURE(S)\n", failures);
    else
        printf("test_bindings: all checks passed\n");
    return failures ? 1 : 0;
}

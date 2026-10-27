/*
 * launcher_bindings.c -- see launcher_bindings.h.
 *
 * The reader is small and forgiving, like the runtime's: it looks for
 * the keys it knows and steps over everything else, so a file written
 * by the Python tool with sources this does not model is read, kept and
 * written back rather than thrown away.
 */
#include "launcher_bindings.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#  include <direct.h>
#  define BIND_MKDIR(p) _mkdir(p)
#else
#  include <sys/stat.h>
#  define BIND_MKDIR(p) mkdir((p), 0755)
#endif

/* The runtime's table, in the runtime's order. Copied rather than
 * shared because the runtime's copy is static to a file that resolves
 * these to indices; the test keeps the two honest. */
const char *const bind_control_names[BIND_CONTROLS] = {
    "a", "b", "x", "y", "black", "white", "start", "back",
    "ltrigger", "rtrigger",
    "dpad_up", "dpad_down", "dpad_left", "dpad_right",
    "lstick_up", "lstick_down", "lstick_left", "lstick_right", "lthumb",
    "rstick_up", "rstick_down", "rstick_left", "rstick_right", "rthumb"
};

const char *const bind_control_labels[BIND_CONTROLS] = {
    "A", "B", "X", "Y", "Black", "White", "Start", "Back",
    "Left trigger", "Right trigger",
    "D-pad up", "D-pad down", "D-pad left", "D-pad right",
    "Left stick up", "Left stick down", "Left stick left", "Left stick right",
    "Left stick press",
    "Right stick up", "Right stick down", "Right stick left", "Right stick right",
    "Right stick press"
};

/* The runtime's DEFAULTS table. Sticks have no keyboard default: every
 * comfortable key is already a button. */
static const struct { const char *pad, *key; } k_defaults[BIND_CONTROLS] = {
    { "pad:a",          "key:Z"      },   /* a */
    { "pad:b",          "key:X"      },   /* b */
    { "pad:x",          "key:A"      },   /* x */
    { "pad:y",          "key:S"      },   /* y */
    { "pad:lshoulder",  "key:W"      },   /* black */
    { "pad:rshoulder",  "key:Q"      },   /* white */
    { "pad:start",      "key:RETURN" },   /* start */
    { "pad:back",       "key:BACK"   },   /* back */
    { "pad:lt",         "key:E"      },   /* ltrigger */
    { "pad:rt",         "key:R"      },   /* rtrigger */
    { "pad:dpad_up",    "key:UP"     },
    { "pad:dpad_down",  "key:DOWN"   },
    { "pad:dpad_left",  "key:LEFT"   },
    { "pad:dpad_right", "key:RIGHT"  },
    { "pad:ly+",        ""           },
    { "pad:ly-",        ""           },
    { "pad:lx-",        ""           },
    { "pad:lx+",        ""           },
    { "pad:lthumb",     ""           },
    { "pad:ry+",        ""           },
    { "pad:ry-",        ""           },
    { "pad:rx-",        ""           },
    { "pad:rx+",        ""           },
    { "pad:rthumb",     ""           }
};

void bind_defaults(BindConfig *c)
{
    int p, i;

    memset(c, 0, sizeof *c);
    for (p = 0; p < BIND_PORTS; p++) {
        c->port[p].device   = BIND_DEV_PAD;
        c->port[p].pad      = p;
        c->port[p].deadzone = 7849;          /* XInput's own left-stick value */
        for (i = 0; i < BIND_CONTROLS; i++) {
            snprintf(c->port[p].pad_src[i], BIND_SOURCE_LEN, "%s", k_defaults[i].pad);
            /* The keyboard column is port 1 only, as in the runtime. */
            snprintf(c->port[p].key_src[i], BIND_SOURCE_LEN, "%s",
                     p == 0 ? k_defaults[i].key : "");
        }
    }
}

int bind_config_path(char *out, size_t n)
{
    const char *env = getenv("RECOMP_INPUT_CONFIG");
#if defined(_WIN32)
    const char *home = getenv("APPDATA");
    const char *tail = "\\xboxrecomp\\input_bindings.json";
#else
    const char *home = getenv("XDG_CONFIG_HOME");
    const char *tail = "/xboxrecomp/input_bindings.json";
#endif

    if (env && *env) {
        snprintf(out, n, "%s", env);
        return 1;
    }
    if (!home || !*home)
        return 0;
    snprintf(out, n, "%s%s", home, tail);
    return 1;
}

/* ------------------------------------------------------------ reading */

static const char *ws(const char *p)
{
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
        p++;
    return p;
}

static const char *jstring(const char *p, char *out, size_t n)
{
    size_t k = 0;

    p = ws(p);
    if (*p != '"')
        return NULL;
    p++;
    while (*p && *p != '"') {
        if (*p == '\\' && p[1]) {
            p++;
            if (k + 1 < n) out[k++] = *p;
        } else if (k + 1 < n) {
            out[k++] = *p;
        }
        p++;
    }
    if (*p != '"')
        return NULL;
    out[k] = '\0';
    return p + 1;
}

/* Step over any one value, whatever it is: string, number, literal,
 * object or array. Needed because the file may carry keys this does not
 * model and stepping over them is how they survive being read. */
static const char *jskip(const char *p)
{
    char dummy[2];
    int depth = 0;

    p = ws(p);
    if (*p == '"')
        return jstring(p, dummy, sizeof dummy);

    if (*p == '{' || *p == '[') {
        while (*p) {
            if (*p == '"') {
                const char *q = jstring(p, dummy, sizeof dummy);

                if (!q)
                    return NULL;
                p = q;
                continue;
            }
            if (*p == '{' || *p == '[')
                depth++;
            else if (*p == '}' || *p == ']') {
                depth--;
                if (depth == 0)
                    return p + 1;
            }
            p++;
        }
        return NULL;
    }

    /* A number, or true/false/null: runs until the value ends. */
    while (*p && *p != ',' && *p != '}' && *p != ']')
        p++;
    return p;
}

static int control_index(const char *name)
{
    int i;

    for (i = 0; i < BIND_CONTROLS; i++)
        if (strcmp(bind_control_names[i], name) == 0)
            return i;
    return -1;
}

static void set_device(BindPort *bp, const char *name)
{
    if (strcmp(name, "keyboard") == 0) {
        bp->device = BIND_DEV_KEYBOARD;
        bp->pad = -1;
    } else if (strncmp(name, "gamepad:", 8) == 0 || strncmp(name, "xinput:", 7) == 0) {
        /* "xinput:N" is what files written before SDL3 say; same slot. */
        bp->device = BIND_DEV_PAD;
        bp->pad = (int)strtol(strchr(name, ':') + 1, NULL, 10);
        if (bp->pad < 0 || bp->pad >= BIND_PORTS)
            bp->pad = 0;
    } else {
        bp->device = BIND_DEV_NONE;
        bp->pad = -1;
    }
}

/* The sources for one control. The first pad source and the first key
 * source are the two the launcher shows; any beyond those are the
 * Python tool's business and are left alone by not being read -- a save
 * from here writes the two it models, which is the shape the defaults
 * have. */
static const char *parse_sources(const char *p, BindPort *bp, int k)
{
    char value[BIND_SOURCE_LEN];
    int got_pad = 0, got_key = 0;

    p = ws(p);
    if (k >= 0) {
        bp->pad_src[k][0] = '\0';
        bp->key_src[k][0] = '\0';
    }
    if (*p == '[') {
        p = ws(p + 1);
        while (*p && *p != ']') {
            p = jstring(p, value, sizeof value);
            if (!p)
                return NULL;
            if (k >= 0) {
                if (strncmp(value, "pad:", 4) == 0 && !got_pad) {
                    snprintf(bp->pad_src[k], BIND_SOURCE_LEN, "%s", value);
                    got_pad = 1;
                } else if (strncmp(value, "key:", 4) == 0 && !got_key) {
                    snprintf(bp->key_src[k], BIND_SOURCE_LEN, "%s", value);
                    got_key = 1;
                }
            }
            p = ws(p);
            if (*p == ',')
                p = ws(p + 1);
        }
        if (*p != ']')
            return NULL;
        return p + 1;
    }
    /* A bare string is allowed too. */
    p = jstring(p, value, sizeof value);
    if (!p)
        return NULL;
    if (k >= 0) {
        if (strncmp(value, "key:", 4) == 0)
            snprintf(bp->key_src[k], BIND_SOURCE_LEN, "%s", value);
        else
            snprintf(bp->pad_src[k], BIND_SOURCE_LEN, "%s", value);
    }
    return p;
}

static const char *parse_bindings_obj(const char *p, BindPort *bp)
{
    char key[48];

    p = ws(p);
    if (*p != '{')
        return NULL;
    p = ws(p + 1);
    while (*p && *p != '}') {
        p = jstring(p, key, sizeof key);
        if (!p)
            return NULL;
        p = ws(p);
        if (*p != ':')
            return NULL;
        p = parse_sources(ws(p + 1), bp, control_index(key));
        if (!p)
            return NULL;
        p = ws(p);
        if (*p == ',')
            p = ws(p + 1);
    }
    return *p == '}' ? p + 1 : NULL;
}

static const char *parse_controller(const char *p, BindConfig *c, int index)
{
    BindPort tmp;
    char key[48], value[64];
    int port = index;

    if (index < BIND_PORTS)
        tmp = c->port[index];
    else
        tmp = c->port[0];

    p = ws(p);
    if (*p != '{')
        return NULL;
    p = ws(p + 1);
    while (*p && *p != '}') {
        p = jstring(p, key, sizeof key);
        if (!p)
            return NULL;
        p = ws(p);
        if (*p != ':')
            return NULL;
        p = ws(p + 1);
        if (strcmp(key, "port") == 0) {
            port = (int)strtol(p, NULL, 10) - 1;        /* the file is 1-based */
            p = jskip(p);
        } else if (strcmp(key, "device") == 0) {
            p = jstring(p, value, sizeof value);
            if (!p) return NULL;
            set_device(&tmp, value);
        } else if (strcmp(key, "deadzone") == 0) {
            tmp.deadzone = (int)strtol(p, NULL, 10);
            p = jskip(p);
        } else if (strcmp(key, "bindings") == 0) {
            p = parse_bindings_obj(p, &tmp);
            if (!p) return NULL;
        } else {
            p = jskip(p);
            if (!p) return NULL;
        }
        p = ws(p);
        if (*p == ',')
            p = ws(p + 1);
    }
    if (*p != '}')
        return NULL;
    if (port >= 0 && port < BIND_PORTS)
        c->port[port] = tmp;
    return p + 1;
}

int bind_load(const char *path, BindConfig *c)
{
    FILE *f;
    long len;
    char *text;
    const char *p;
    char key[48];
    int index = 0, ok = 0;

    bind_defaults(c);
    f = fopen(path, "rb");
    if (!f)
        return 0;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len <= 0 || len > 1 << 20) { fclose(f); return 0; }
    text = (char *)malloc((size_t)len + 1);
    if (!text) { fclose(f); return 0; }
    if (fread(text, 1, (size_t)len, f) != (size_t)len) {
        fclose(f); free(text); return 0;
    }
    fclose(f);
    text[len] = '\0';

    p = ws(text);
    if (*p != '{') { free(text); return 0; }
    p = ws(p + 1);
    while (*p && *p != '}') {
        p = jstring(p, key, sizeof key);
        if (!p) break;
        p = ws(p);
        if (*p != ':') break;
        p = ws(p + 1);
        if (strcmp(key, "pad_api") == 0 && *p == '"') {
            char api[16];
            p = jstring(p, api, sizeof api);
            if (!p) break;
            c->pad_api = strcmp(api, "xinput") == 0 ? BIND_PAD_API_XINPUT : BIND_PAD_API_SDL;
        } else if (strcmp(key, "controllers") == 0 && *p == '[') {
            p = ws(p + 1);
            while (*p && *p != ']') {
                p = parse_controller(p, c, index++);
                if (!p) break;
                p = ws(p);
                if (*p == ',')
                    p = ws(p + 1);
            }
            if (!p) break;
            if (*p == ']') p++;
            ok = 1;
        } else {
            p = jskip(p);
            if (!p) break;
        }
        p = ws(p);
        if (*p == ',')
            p = ws(p + 1);
    }
    free(text);
    return ok;
}

/* ------------------------------------------------------------ writing */

static void make_parents(const char *path)
{
    char buf[1024];
    size_t i;

    snprintf(buf, sizeof buf, "%s", path);
    for (i = 0; buf[i]; i++)
        if (buf[i] == '\\' || buf[i] == '/') {
            char ch = buf[i];
            buf[i] = '\0';
            if (i > 0) BIND_MKDIR(buf);
            buf[i] = ch;
        }
}

int bind_save(const char *path, const BindConfig *c)
{
    FILE *f;
    int p, i;

    make_parents(path);
    f = fopen(path, "wb");
    if (!f)
        return 0;

    fprintf(f, "{\n  \"version\": 1,\n  \"pad_api\": \"%s\",\n  \"controllers\": [\n",
            c->pad_api == BIND_PAD_API_XINPUT ? "xinput" : "sdl");
    for (p = 0; p < BIND_PORTS; p++) {
        const BindPort *bp = &c->port[p];
        int first = 1;

        fprintf(f, "    {\n      \"port\": %d,\n", p + 1);
        if (bp->device == BIND_DEV_KEYBOARD)
            fprintf(f, "      \"device\": \"keyboard\",\n");
        else if (bp->device == BIND_DEV_PAD)
            fprintf(f, "      \"device\": \"gamepad:%d\",\n", bp->pad);
        else
            fprintf(f, "      \"device\": \"none\",\n");
        fprintf(f, "      \"deadzone\": %d,\n      \"bindings\": {\n", bp->deadzone);

        for (i = 0; i < BIND_CONTROLS; i++) {
            const char *pd = bp->pad_src[i], *ky = bp->key_src[i];

            /* Every control is written, including one with no sources.
             * Leaving it out does not unbind it: a control the file does
             * not name keeps its default, here and in the runtime, so an
             * omitted control comes back bound. An empty array is how
             * "deliberately nothing" is said. */
            if (!first)
                fprintf(f, ",\n");
            first = 0;
            fprintf(f, "        \"%s\": [", bind_control_names[i]);
            if (*pd) fprintf(f, "\"%s\"", pd);
            if (*pd && *ky) fprintf(f, ", ");
            if (*ky) fprintf(f, "\"%s\"", ky);
            fprintf(f, "]");
        }
        fprintf(f, "\n      }\n    }%s\n", p + 1 < BIND_PORTS ? "," : "");
    }
    fprintf(f, "  ]\n}\n");
    fclose(f);
    return 1;
}

/* ------------------------------------------------------------- labels */

static const struct { const char *src, *label; } k_labels[] = {
    { "pad:a", "A" }, { "pad:b", "B" }, { "pad:x", "X" }, { "pad:y", "Y" },
    { "pad:lshoulder", "Left shoulder" }, { "pad:rshoulder", "Right shoulder" },
    { "pad:start", "Start" }, { "pad:back", "Back" },
    { "pad:lthumb", "Left stick press" }, { "pad:rthumb", "Right stick press" },
    { "pad:dpad_up", "D-pad up" }, { "pad:dpad_down", "D-pad down" },
    { "pad:dpad_left", "D-pad left" }, { "pad:dpad_right", "D-pad right" },
    { "pad:lt", "Left trigger" }, { "pad:rt", "Right trigger" },
    { "pad:lx-", "Left stick left" }, { "pad:lx+", "Left stick right" },
    { "pad:ly+", "Left stick up" }, { "pad:ly-", "Left stick down" },
    { "pad:rx-", "Right stick left" }, { "pad:rx+", "Right stick right" },
    { "pad:ry+", "Right stick up" }, { "pad:ry-", "Right stick down" },
    { "key:RETURN", "Enter" }, { "key:BACK", "Backspace" },
    { "key:UP", "Up arrow" }, { "key:DOWN", "Down arrow" },
    { "key:LEFT", "Left arrow" }, { "key:RIGHT", "Right arrow" },
    { "key:SPACE", "Space" }, { "key:ESCAPE", "Escape" },
    { "key:LSHIFT", "Left shift" }, { "key:RSHIFT", "Right shift" },
    { "key:LCTRL", "Left ctrl" }, { "key:RCTRL", "Right ctrl" },
    { "key:TAB", "Tab" },
    { NULL, NULL }
};

const char *bind_source_label(const char *src)
{
    static char buf[48];
    int i;

    if (!src || !*src)
        return "Not bound";
    for (i = 0; k_labels[i].src; i++)
        if (strcmp(k_labels[i].src, src) == 0)
            return k_labels[i].label;
    if (strncmp(src, "key:", 4) == 0) {
        snprintf(buf, sizeof buf, "%s", src + 4);
        return buf;
    }
    snprintf(buf, sizeof buf, "%s", src);
    return buf;
}

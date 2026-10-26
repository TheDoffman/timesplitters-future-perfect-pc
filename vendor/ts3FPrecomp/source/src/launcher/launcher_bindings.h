/*
 * launcher_bindings.h -- the input config, as the launcher edits it.
 *
 * The runtime reads this same file (src/input/input_bindings.c) and
 * resolves every string to a key code or a pad index at load. The
 * launcher needs the strings themselves, to show them and to change
 * them, so it keeps its own view: the file's text, not the runtime's
 * resolved form.
 *
 * The vocabulary is the runtime's and nothing here may invent any of
 * it. A control the runtime does not know is a binding that silently
 * does nothing, which is worse than refusing to write it, so the names
 * below are the runtime's table copied deliberately and checked against
 * it by tools/input_ui/test_runtime_vocabulary.py.
 *
 * Bindings belong to the person, not the title: one file, shared by
 * every game the toolkit builds, in the place the runtime looks.
 */
#ifndef XBOXRECOMP_LAUNCHER_BINDINGS_H
#define XBOXRECOMP_LAUNCHER_BINDINGS_H

#include <stddef.h>

#define BIND_PORTS      4
#define BIND_CONTROLS   24
#define BIND_SOURCE_LEN 32

/* The 24 Xbox inputs, in the order the launcher lists them, by the names
 * the config file uses. */
extern const char *const bind_control_names[BIND_CONTROLS];
/* The same, as a player would read them. */
extern const char *const bind_control_labels[BIND_CONTROLS];

typedef enum { BIND_DEV_NONE, BIND_DEV_PAD, BIND_DEV_KEYBOARD } BindDevice;

typedef struct BindPort {
    BindDevice device;
    int        pad;                       /* pad slot, 0..3 */
    int        deadzone;
    /* One pad source and one keyboard source per control, which is the
     * shape the defaults have and the shape a player thinks in. A file
     * with more sources than that keeps them: see `extra`. */
    char pad_src[BIND_CONTROLS][BIND_SOURCE_LEN];
    char key_src[BIND_CONTROLS][BIND_SOURCE_LEN];
} BindPort;

/* Which API reads the pads, for every port and every game: SDL3 reads
 * every kind of controller, XInput only Xbox ones. The file's "pad_api". */
enum { BIND_PAD_API_SDL = 0, BIND_PAD_API_XINPUT = 1 };

typedef struct BindConfig {
    BindPort port[BIND_PORTS];
    int      pad_api;                     /* BIND_PAD_API_* */
} BindConfig;

/* The built-in mapping, the same one the runtime falls back to with no
 * file at all. */
void bind_defaults(BindConfig *c);

/* The per-user file both ends agree on: RECOMP_INPUT_CONFIG if set,
 * else %APPDATA%\xboxrecomp\input_bindings.json. */
int  bind_config_path(char *out, size_t n);

/* Read and write that schema. A read that finds no file leaves the
 * defaults in place and returns 0. */
int  bind_load(const char *path, BindConfig *c);
int  bind_save(const char *path, const BindConfig *c);

/* A source string as a player would read it: "pad:lx+" -> "Left stick
 * left", "key:RETURN" -> "Enter", "" -> "Not bound". */
const char *bind_source_label(const char *src);

#endif /* XBOXRECOMP_LAUNCHER_BINDINGS_H */

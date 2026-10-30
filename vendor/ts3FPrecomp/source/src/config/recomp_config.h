/*
 * recomp_config.h -- the settings a player chooses, and where they live.
 *
 * Everything the runtime can be told is an environment variable, which is
 * right for a developer and no use to someone who just wants the game in
 * widescreen. This is the other half: a small file the launcher writes and
 * the runtime reads, so a setting survives being chosen once.
 *
 * The environment still wins. A variable set on the command line, in a
 * .bat, or by CI overrides the file, so every existing way of driving the
 * runtime keeps working and the file is what happens when nothing said
 * otherwise. Callers say both names at the point of use:
 *
 *     v = recomp_config_lookup("RECOMP_RES_SCALE", "resolution_scale");
 *
 * which keeps the precedence visible where the setting is read rather
 * than buried here.
 *
 * The file is per title, keyed by the title id out of the XBE
 * certificate: a resolution scale is a judgement about one game on one
 * machine, and the Hor+ register is meaningless to any other game. Input
 * bindings are deliberately not here -- those belong to the person, not
 * the title, and they already have a file of their own.
 *
 * It is an INI rather than JSON because the settings are flat and because
 * a generated INI can explain itself. The file a player opens says what
 * each line does; JSON has nowhere to put that.
 */
#ifndef XBOXRECOMP_RECOMP_CONFIG_H
#define XBOXRECOMP_RECOMP_CONFIG_H

#include <stddef.h>
#include <stdint.h>

/* The running title, from its XBE certificate, which decides the file's
 * name. Called during start-up before anything reads a setting; without
 * it the lookups below still work and simply find no file. */
void recomp_config_set_title(uint32_t title_id);
/* The title id given above, 0 before it is known. */
uint32_t recomp_config_title_id(void);
/* The per-user directory settings live in (%APPDATA%\xboxrecomp on
 * Windows); 0 if there is none. Caches go beneath it. */
int recomp_config_user_dir(char *out, size_t n);

/* The value for a setting: the environment variable if it is set and not
 * empty, then the config file, then NULL. Either name may be NULL to skip
 * that source. The returned string is owned here and valid for the life
 * of the process. */
const char *recomp_config_lookup(const char *env_name, const char *key);

/* The same, parsed, with a fallback for absent or unreadable values.
 * Booleans accept 1/0, on/off, yes/no, true/false, and an empty
 * environment variable counts as on -- the rule xbox_EnvSwitch already
 * uses, so a variable behaves the same whether or not this file exists. */
int    recomp_config_int(const char *env_name, const char *key, int fallback);
int    recomp_config_bool(const char *env_name, const char *key, int fallback);
double recomp_config_float(const char *env_name, const char *key, double fallback);

/* The file the lookups read, for a log line or a launcher; NULL when no
 * file was found. */
const char *recomp_config_path(void);

/* ------------------------------------------------------------ settings */

/* The settings a player chooses, as one thing, so the launcher that
 * writes the file and the runtime that reads it cannot drift apart on
 * what the file contains. The runtime still reads values one at a time
 * through the lookups above -- that keeps the environment override
 * visible where each setting is used -- but the file's shape is here. */
typedef struct RecompSettings {
    int  resolution_scale;      /* 1..8 */
    int  widescreen;            /* 0 or 1 */
    double hor_plus;            /* 0 leaves the camera alone; 0.75 is 4:3->16:9 */
    int  hor_plus_register;     /* per title; 60 for TimeSplitters 2 */
    char widescreen_2d[16];     /* "auto" or "centre": where 2D goes in widescreen */
    int  anisotropy;            /* 1..16 */
    char frame_cap[16];         /* "adaptive", "60", "30", "0" */
    int  fps_overlay;           /* 0 or 1 */
    int  fullscreen;            /* 0 or 1: borderless, on the window's monitor */
    int  vrr;                   /* 0 or 1: present for variable refresh when fullscreen */
    int  frame_interp;          /* 1 (off) to 4: frames shown for each the game draws */
    char game_dir[512];         /* empty means beside the executable */
} RecompSettings;

void recomp_settings_defaults(RecompSettings *s);

/* Read the file at `path` into `s`, starting from the defaults. Absent
 * keys keep their default, so an older file gains new settings rather
 * than losing them. Returns 0 if the file could not be read at all. */
int  recomp_settings_read(const char *path, RecompSettings *s);

/* Write `s` to `path`, comments and all, creating the directories above
 * it. Returns 0 on failure. This is the only place the file's text is
 * written, by the launcher and by the runtime's first-run default
 * alike. */
int  recomp_settings_write(const char *path, const RecompSettings *s);

/* Where this title's file belongs: RECOMP_DISPLAY_CONFIG if set,
 * otherwise the per-user path for `title_id`. Returns 0 if there is
 * nowhere sensible to put it. */
int  recomp_settings_path(uint32_t title_id, char *out, size_t n);

#endif /* XBOXRECOMP_RECOMP_CONFIG_H */

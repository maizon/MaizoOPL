"""Build the actual cheatconfig.c using small host stubs for OPL config storage."""
import pathlib
import subprocess
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as name:
    d = pathlib.Path(name)
    (d / "config.h").write_text(r'''
#ifndef CONFIG_H
#define CONFIG_H
#define CONFIG_KEY_NAME_LEN 32
#define CONFIG_KEY_VALUE_LEN 256
#define CONFIG_ITEM_CHEAT_SELECTION_PREFIX "$CheatSel"
typedef struct { char values[250][256]; } config_set_t;
int configGetStrCopy(config_set_t *, const char *, char *, int);
void configSetStr(config_set_t *, const char *, const char *);
void configRemoveKey(config_set_t *, const char *);
#endif
''')
    (d / "cheatconfig.h").write_text(r'''
#include "config.h"
#define NUL 0
#define SPACE 32
#define CHEAT_NAME_MAX 128
#define MAX_CODES 250
#define MAX_CHEATLIST 510
typedef struct { unsigned int addr, val; } code_t;
typedef struct { char name[129]; code_t codes[MAX_CHEATLIST]; int enabled; } cheat_entry_t;
extern cheat_entry_t gCheats[MAX_CODES];
int cheatConfigIsMasterCode(const char *);
void cheatConfigLoadSelections(config_set_t *);
void cheatConfigSaveSelections(config_set_t *);
''')
    (d / "test.c").write_text(r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cheatconfig.h"
cheat_entry_t gCheats[MAX_CODES];
static config_set_t cfg;
static int indexOf(const char *key) { return atoi(key + 9); }
int configGetStrCopy(config_set_t *c, const char *key, char *out, int len) {
    const char *v = c->values[indexOf(key)];
    snprintf(out, len, "%s", v);
    return *v != 0;
}
void configSetStr(config_set_t *c, const char *key, const char *v) {
    snprintf(c->values[indexOf(key)], 256, "%s", v);
}
void configRemoveKey(config_set_t *c, const char *key) { c->values[indexOf(key)][0] = 0; }
int main(void) {
    strcpy(gCheats[0].name, "Master Code");
    strcpy(gCheats[1].name, "Infinite Health");
    strcpy(gCheats[2].name, "Infinite Health");
    gCheats[1].codes[0] = (code_t){0x20111111, 1};
    gCheats[2].codes[0] = (code_t){0x20222222, 2};
    gCheats[2].enabled = 1;
    cheatConfigSaveSelections(&cfg);
    assert(cfg.values[0][0] == '@');
    cheatConfigLoadSelections(&cfg);
    assert(gCheats[0].enabled && !gCheats[1].enabled && gCheats[2].enabled);
    code_t temp = gCheats[1].codes[0];
    gCheats[1].codes[0] = gCheats[2].codes[0]; gCheats[2].codes[0] = temp;
    cheatConfigLoadSelections(&cfg);
    assert(gCheats[1].enabled && !gCheats[2].enabled);
    strcpy(cfg.values[0], "Infinite Health");
    cheatConfigLoadSelections(&cfg);
    assert(!gCheats[1].enabled && !gCheats[2].enabled);
    strcpy(gCheats[2].name, "Other");
    cheatConfigLoadSelections(&cfg);
    assert(gCheats[1].enabled && !gCheats[2].enabled);
    puts("cheat selection identity passed");
    return 0;
}
'''.replace("key + 9", "key + 9"))
    subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", "-I" + str(d), str(root / "src/cheatconfig.c"), str(d / "test.c"), "-o", str(d / "test")], check=True)
    subprocess.run([str(d / "test")], check=True)

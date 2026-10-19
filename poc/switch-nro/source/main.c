#include <switch.h>
#include <stdio.h>
#include <stdlib.h>

#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"

#define SCRIPT_PATH "sdmc:/switch/TheModdingOfIsaac/test.lua"
#define MAX_SCRIPT_BYTES (1024 * 1024)

static char *read_script(const char *path, size_t *length) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) return NULL;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return NULL;
    }
    long size = ftell(file);
    if (size < 0 || (unsigned long)size > MAX_SCRIPT_BYTES) {
        fclose(file);
        return NULL;
    }

    if (fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return NULL;
    }

    char *buffer = (char *)malloc((size_t)size + 1);
    if (buffer == NULL) {
        fclose(file);
        return NULL;
    }

    size_t read_count = fread(buffer, 1, (size_t)size, file);
    fclose(file);
    if (read_count != (size_t)size) {
        free(buffer);
        return NULL;
    }

    buffer[read_count] = '\0';
    *length = read_count;
    return buffer;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    consoleInit(NULL);
    printf("The Modding of Isaac - Specialist mock POC\n");
    printf("Standalone Lua 5.1 test; not an Isaac mod\n");
    printf("-------------------------------------------\n");
    printf("Script: %s\n\n", SCRIPT_PATH);

    lua_State *state = luaL_newstate();
    if (state == NULL) {
        printf("ERROR: luaL_newstate failed.\n");
    } else {
        luaL_openlibs(state);

        size_t script_length = 0;
        char *script = read_script(SCRIPT_PATH, &script_length);
        if (script == NULL) {
            printf("ERROR: cannot read script (missing, too large, or I/O failure).\n");
        } else {
            int status = luaL_loadbuffer(state, script, script_length, "@" SCRIPT_PATH);
            free(script);

            if (status == 0) {
                status = lua_pcall(state, 0, LUA_MULTRET, 0);
            }

            if (status != 0) {
                const char *error = lua_tostring(state, -1);
                printf("\nLUA ERROR: %s\n", error != NULL ? error : "(non-string error)");
            } else {
                printf("\nLua test script completed.\n");
            }
        }

        lua_close(state);
    }

    PadState pad;
    padInitializeDefault(&pad);
    printf("\nPress PLUS to exit.\n");
    while (appletMainLoop()) {
        padUpdate(&pad);
        if (padGetButtonsDown(&pad) & HidNpadButton_Plus) break;
        consoleUpdate(NULL);
    }

    consoleExit(NULL);
    return 0;
}

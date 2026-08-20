#include "app_launch.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char *app_launch_params(int argc, char *argv[]) {
    if (argc < 2 || argv[1] == NULL) {
        return NULL;
    }
    return argv[1][0] == '{' ? argv[1] : NULL;
}

bool app_launch_param_string(const char *params, const char *key, char *out, size_t out_len) {
    if (params == NULL || out_len == 0) {
        return false;
    }

    char needle[64];
    int needle_len = snprintf(needle, sizeof(needle), "\"%s\"", key);
    if (needle_len <= 0 || (size_t) needle_len >= sizeof(needle)) {
        return false;
    }
    const char *found = strstr(params, needle);
    if (found == NULL) {
        return false;
    }

    const char *cursor = found + needle_len;
    while (isspace((unsigned char) *cursor)) {
        cursor++;
    }
    if (*cursor++ != ':') {
        return false;
    }
    while (isspace((unsigned char) *cursor)) {
        cursor++;
    }
    if (*cursor++ != '"') {
        return false;
    }

    /* Same deliberate shortcut as json_scan: enough for machine-generated parameters,
     * and not a JSON parser. Backslash escapes are passed through as-is. */
    size_t written = 0;
    while (*cursor != '\0' && *cursor != '"' && written + 1 < out_len) {
        out[written++] = *cursor++;
    }
    out[written] = '\0';
    return *cursor == '"';
}

bool app_log_to_file(const char *path) {
    FILE *log = freopen(path, "w", stderr);
    if (log == NULL) {
        return false;
    }
    setvbuf(stderr, NULL, _IOLBF, 0);
    dup2(fileno(stderr), fileno(stdout));
    return true;
}

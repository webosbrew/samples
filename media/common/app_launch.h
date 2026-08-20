/*
 * Two things every native webOS app has to deal with, and neither is obvious.
 *
 * 1. The app manager launches a native app with its launch parameters as a single JSON
 *    string in argv[1] - even when nothing was passed, the TV may supply something like
 *    {"displayAffinity":0}. An option parser that rejects unknown arguments will refuse
 *    to start, exit before printing anything anyone can see, and look for all the world
 *    like the app crashed on launch.
 *
 * 2. There is no console. stdout and stderr of an app-manager-launched native app go
 *    nowhere, so a sample that only logs to stderr cannot be debugged on the device at
 *    all. Passing a log path in the launch parameters fixes that:
 *
 *      ares-launch -d <device> <appid> -p '{"log":"/tmp/sample.log"}'
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Returns argv[1] when it looks like a launch-parameters object, otherwise NULL. */
const char *app_launch_params(int argc, char *argv[]);

/* Reads a string value out of the launch parameters. */
bool app_launch_param_string(const char *params, const char *key, char *out, size_t out_len);

/* Points stdout and stderr at `path`, truncating it. */
bool app_log_to_file(const char *path);

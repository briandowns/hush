#include <inttypes.h>
#ifdef __linux__
#include <signal.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ulfius.h>

#include "api.h"
#include "config.h"
#include "database.h"
#include "dot_env.h"
#include "logger.h"

#define STR1(x) #x
#define STR(x) STR1(x)

#define USAGE                                                                                                          \
    "usage: %s [-vh] -c [config]\n\
    -c          config file [JSON format]\n\
    -v          version\n\
    -h          help\n"

int control = 0;

/**
 * sig_handler captures ctrl-c
 */
static void
sig_handler(int dummy)
{
    control = 1;
    exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
    signal(SIGINT, sig_handler);

    s_log_init(stdout);

    int c;
    if (argc > 1) {
        while ((c = getopt(argc, argv, "hv")) != -1) {
            switch (c) {
                case 'h':
                    printf(USAGE, STR(app_name));
                    return 0;
                case 'v':
                    printf("%s - git: %s\n", STR(app_name), STR(git_sha));
                    return 0;
                default:
                    printf(USAGE, STR(app_name));
                    return 1;
            }
        }
    } else {
        printf(USAGE, STR(app_name));
        return 1;
    }

    env_load("config.env", 0);

    char *server = "192.168.0.60";
    char *user = "pass";
    char *password = "one4all";
    char *database = "pass";

    db_t *db = db_new();

    s_log(LOG_INFO, s_log_string("msg", "initializing database"));

    int res = db_init(db, server, user, password, database);
    if (res != 0) {
        fprintf(stderr, "error: db init - %s\n", db_get_error(db));
        return 1;
    }
    
    api_init(db);
    api_start();

    config_free(conf);
    db_cleanup(db);

    return 0;
}

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
#include "database.h"
#include "dot_env.h"
#include "logger.h"

#define STR1(x) #x
#define STR(x) STR1(x)


int control = 0;

/**
 * sig_handler captures ctrl-c.
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
    srand(time(NULL));

    s_log_init(stdout);

    env_load("config.env", 0);
    printf("XXX - now into the db\n");
    db_t *db = db_new();

    s_log(LOG_INFO, s_log_string("msg", "initializing database"));
    
    int res = db_init(db, getenv("DB_HOST"), getenv("DB_USER"), getenv("DB_PASS"), getenv("DB_NAME"));
    if (res != 0) {
        fprintf(stderr, "error: db init - %s\n", db_get_error(db));
        return 1;
    }
    
    api_init(db);
    api_start();

    db_cleanup(db);

    return 0;
}

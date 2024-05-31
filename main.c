#include <inttypes.h>
#ifdef __linux__
#include <signal.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <ulfius.h>

#include "api.h"
#include "database.h"

#define STR1(x) #x
#define STR(x) STR1(x)

int control = 0;

/**
 * sig_handler captures ctrl-c
 */
static void sig_handler(int dummy) {
    control = 1;
    exit(EXIT_FAILURE);
}

int main(int argc, char **argv) {
    signal(SIGINT, sig_handler);

    char *server = "192.168.0.60";
    char *user = "pass";
    char *password = "one4all";
    char *database = "pass";

    db_t *db = db_new();

    int res = db_init(db, server, user, password, database);
    if (res != 0) {
        fprintf(stderr, "error: db init - %s\n", db_get_error(db));
        return 1;
    }

    // user_t **users = malloc(sizeof(user_t));
    // uint64_t count = db_get_all_users(db, users);
    // printf("user count: %" PRId64 "\n", count);

    // for (uint64_t i = 0; i < count; i++) {
    //     printf("id: %lu, user: %s, first: %s, last: %s, pass hash: %s\n",
    //         users[i]->id, users[i]->username, users[i]->first_name, users[i]->last_name, users[i]->password);
    // }

    // db_free_users(users, count);

    user_t *usr = db_user_new();
    res = db_get_user_by_username(db, "bdowns", usr);
    if (res == -1) {
        fprintf(stderr, "error: db get by username - %s\n", db_get_error(db));
        return 1;
    }
    if (res == 0) {
        printf("INFO: no result found\n");
    } else {
        printf("get user by name - id: %lu, user: %s, first: %s, last: %s, pass hash: %s\n",
            usr->id, usr->username, usr->first_name, usr->last_name, usr->password);
    }
    db_free_user(usr);

    //db_add_password(db, "gmail", "Jesus1234!", 1);

    password_t *pass = malloc(sizeof(password_t));
    res = db_get_password_by_name(db, "gmail", 2, pass) ;
    if (res == -1) {
        fprintf(stderr, "error: db get by username - %s\n", db_get_error(db));
        return 1;
    }
    if (res == 0) {
        printf("INFO: no result found\n");
    } else {
        printf("get pass by name - %lu, %s, %s, %lu\n", pass->id, pass->name, pass->password, pass->user_id);
    }
    
    api_init(db);
    api_start();

    db_cleanup(db);

    return 0;
}

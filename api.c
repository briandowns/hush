#include <jansson.h>
#include <ulfius.h>

#include "database.h"
#include "http.h"
#include "pass.h"

#define DEFAULT_PORT 3000

#define AUTH_HEADER "X-Hush-Auth"

#define API_PATH "/api/v1"
#define HEALTH_PATH  "/healthz"
#define USER_PATH "/user"
#define USERS_PATH "/users"
#define PASSWORD_PATH "/password"
#define PASSWORDS_PATH "/passwords"
#define HEALTH_STATUS_SICK    "sick"
#define HEALTH_STATUS_HEALTHY "healthy"

/**
 * time_spent takes the start time of a route handler
 * and calculates how long it ran for. It then returns
 * that value to be logged.
 */
#define TIME_SPENT(x) {                      \
    clock_t diff = clock() - x;              \
    int msec = diff * 1000 / CLOCKS_PER_SEC; \
    msec % 1000;                             \
}

static struct _u_instance instance;
static db_t *dbr = NULL;

// /**
//  * auth_basic is responsible for basic authentication for
//  * configured endpoints
//  */
// int auth_basic(const struct _u_request *request, struct _u_response *response, void *user_data) {
//     if (request->auth_basic_user != NULL && request->auth_basic_password != NULL &&
//         0 == o_strcmp(request->auth_basic_user, USER) && 0 == o_strcmp(request->auth_basic_password, PASSWORD)) {
//         return U_CALLBACK_CONTINUE;
//     } else {
//         ulfius_set_string_body_response(response, HTTP_STATUS_UNAUTHORIZED, "error authentication");
//         return U_CALLBACK_UNAUTHORIZED;
//     }
// }

int callback_auth_token(const struct _u_request *request, struct _u_response *response, void *user_data) {
    const char *token = u_map_get(request->map_header, AUTH_HEADER);

    return U_CALLBACK_CONTINUE;
}

/**
 * callback_health_check handles all health check
 * requests to the service.
 */
static int callback_health(const struct _u_request *request, struct _u_response *response, void *user_data) {
    clock_t start = clock();
    ulfius_set_string_body_response(response, HTTP_STATUS_OK, "OK");
    
    return U_CALLBACK_CONTINUE;
}

/**
 * callback_default is used to handled calls that don't have
 * a matching route. Returns an expected 404.
 */
int callback_default(const struct _u_request *request, struct _u_response *response, void *user_data) {
    ulfius_set_string_body_response(response, HTTP_STATUS_NOT_FOUND, "page not found");

    return U_CALLBACK_CONTINUE;
}

/**
 * callback_new_user
 */
static int callback_new_user(const struct _u_request *request, struct _u_response *response, void *user_data) {
    clock_t start = clock();

    json_error_t error;
    json_t *json_new_user_request = ulfius_get_json_body_request(request, &error);
    const char *username = json_string_value(json_object_get(json_new_user_request, "username"));
    const char *first_name = json_string_value(json_object_get(json_new_user_request, "first_name"));
    const char *last_name = json_string_value(json_object_get(json_new_user_request, "last_name"));
    const char *password = json_string_value(json_object_get(json_new_user_request, "password"));
    if (strcmp(error.text, "")) {
        printf("error: %s", error.text);
    }

    char * token = generate_password(32);

    if (db_add_user(dbr, username, first_name, last_name, password, token) != 0) {
        printf("error: %s\n", db_get_error(dbr));
        ulfius_set_string_body_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "failed to add new user");
        return U_CALLBACK_ERROR;
    }

    json_t *json_body = json_object();
    json_body = json_pack("{s:s}", "token", token);
    ulfius_set_json_body_response(response, HTTP_STATUS_OK, json_body);

    json_decref(json_new_user_request);
    json_decref(json_body);
    free(token);

    return U_CALLBACK_CONTINUE;
}

/**
 * callback_new_user
 */
static int callback_get_password(const struct _u_request *request, struct _u_response *response, void *user_data) {
    clock_t start = clock();

}

int api_init(db_t *db) {
    dbr = db;

    if (ulfius_init_instance(&instance, DEFAULT_PORT, NULL, NULL) != U_OK) {
        fprintf(stderr, "error ulfius_init_instance, abort\n");
        return EXIT_FAILURE;
    }

    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_GET, HEALTH_PATH, NULL, 0, &callback_health, NULL);

    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_POST, API_PATH USER_PATH, NULL, 0, &callback_new_user, NULL);
    
    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_POST, API_PATH PASSWORD_PATH, NULL, 0, &callback_auth_token, NULL);
    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_POST, API_PATH PASSWORD_PATH, NULL, 1, &callback_new_user, NULL);

    ulfius_set_default_endpoint(&instance, &callback_default, NULL);

    return 0;
}

void api_start() {
    if (ulfius_start_framework(&instance) == U_OK) {
        printf("Start framework on port %d\n", instance.port);

        // Wait for the user to press <enter> on the console to quit the application
        getchar();
    } else {
        fprintf(stderr, "error starting framework\n");
    }

    ulfius_stop_framework(&instance);
    ulfius_clean_instance(&instance);
}
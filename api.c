#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>

#include <jansson.h>
#include <ulfius.h>

#include "database.h"
#include "http.h"
#include "logger.h"
#include "pass.h"

#define DEFAULT_PORT 3000

#define AUTH_HEADER "X-Hush-Auth"

#define API_PATH "/api/v1"
#define HEALTH_PATH  "/healthz"
#define USER_PATH "/user"
#define USERS_PATH "/users"
#define USER_BY_ID_PATH USER_PATH "/:id"
#define PASSWORD_PATH "/password"
#define PASSWORD_BY_NAME_PATH PASSWORD_PATH "/:name"
#define PASSWORDS_PATH "/passwords"
#define HEALTH_STATUS_SICK    "sick"
#define HEALTH_STATUS_HEALTHY "healthy"

/**
 * time_spent takes the start time of a route handler
 * and calculates how long it ran for. It then returns
 * that value to be logged.
 */
#define TIME_SPENT(x)                        \
{                                            \
    clock_t diff = clock() - x;              \
    int msec = diff * 1000 / CLOCKS_PER_SEC; \
    msec % 1000;                             \
}

static struct _u_instance instance;
static db_t *dbr = NULL;

/// @brief logs request and response data
/// @param request 
/// @param response 
/// @param start request time start
void
log_request(const struct _u_request *request, struct _u_response *response, clock_t start)
{
    clock_t diff = clock() - start;
    int msec = diff * 1000;

    s_log(LOG_INFO,
        s_log_string("method", request->http_verb), 
        s_log_string("path", request->url_path),
        //s_log_string("host", ipv4),
        s_log_uint32("status", response->status),
        s_log_string("proto", request->http_protocol),
        s_log_int("duration", msec));
}

// /**
//  * auth_basic is responsible for basic authentication for
//  * configured endpoints
//  */
// int
// auth_basic(const struct _u_request *request, struct _u_response *response, void *user_data)
// {
//     if (request->auth_basic_user != NULL && request->auth_basic_password != NULL &&
//         0 == o_strcmp(request->auth_basic_user, USER) && 0 == o_strcmp(request->auth_basic_password, PASSWORD)) {
//         return U_CALLBACK_CONTINUE;
//     } else {
//         ulfius_set_string_body_response(response, HTTP_STATUS_UNAUTHORIZED, "error authentication");
//         return U_CALLBACK_UNAUTHORIZED;
//     }
// }

int
callback_auth_token(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    const char *token = u_map_get(request->map_header, AUTH_HEADER);

    //char *token = u_map_get(request->map_url, AUTH_HEADER);

    return U_CALLBACK_CONTINUE;
}

/**
 * callback_health_check handles all health check
 * requests to the service.
 */
static int
callback_health(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    clock_t start = clock();
    ulfius_set_string_body_response(response, HTTP_STATUS_OK, "OK");
    
    log_request(request, response, start);
    return U_CALLBACK_CONTINUE;
}

/**
 * callback_default is used to handled calls that don't have
 * a matching route. Returns an expected 404.
 */
int
callback_default(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    ulfius_set_string_body_response(response, HTTP_STATUS_NOT_FOUND, "page not found");

    return U_CALLBACK_CONTINUE;
}

/**
 * callback_new_user
 */
static int
callback_new_user(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    clock_t start = clock();

    const char *auth_token = u_map_get(request->map_header, AUTH_HEADER);

    user_t *user1 = db_user_new();
    int row_count = db_user_get_by_token(dbr, auth_token, user1);
    if (strcmp(user1->username, "admin")) {
        ulfius_set_string_body_response(response, HTTP_STATUS_UNAUTHORIZED, "error authentication");
        return U_CALLBACK_UNAUTHORIZED;
    }
    db_user_free(user1);

    json_error_t error;
    json_t *json_new_user_request = ulfius_get_json_body_request(request, &error);
    const char *username = json_string_value(json_object_get(json_new_user_request, "username"));
    const char *first_name = json_string_value(json_object_get(json_new_user_request, "first_name"));
    const char *last_name = json_string_value(json_object_get(json_new_user_request, "last_name"));
    const char *password = json_string_value(json_object_get(json_new_user_request, "password"));
    if (strcmp(error.text, "")) {
        printf("error: %s", error.text);
    }

    char *token = generate_password(32);

    if (db_user_add(dbr, username, first_name, last_name, password, token) != 0) {
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

    log_request(request, response, start);
    return U_CALLBACK_CONTINUE;
}

/**
 * callback_new_user
 */
static int
callback_get_users(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    clock_t start = clock();

    const char *auth_token = u_map_get(request->map_header, AUTH_HEADER);

    user_t *user1 = db_user_new();
    int row_count = db_user_get_by_token(dbr, auth_token, user1);
    if (strcmp(user1->username, "admin")) {
        ulfius_set_string_body_response(response, HTTP_STATUS_UNAUTHORIZED, "error authentication");
        return U_CALLBACK_UNAUTHORIZED;
    }
    db_user_free(user1);

    user_t **users = db_users_new();
    uint64_t user_count = db_users_get_all(dbr, users);

    json_t *json_users = json_array();
    for (int i = 0; i < user_count; i++) {
        if (users[i] != NULL) {
            json_t *ju = json_pack("{s:i, s:s, s:s}", "id", users[i]->id, "first_name", users[i]->first_name, "last_name", users[i]->last_name);
            json_array_append_new(json_users, ju);
        }
    }

    json_t *json_body = json_object();
    json_body = json_pack("{s:i, s:O}", "count", user_count, "users", json_users);

    ulfius_set_json_body_response(response, HTTP_STATUS_OK, json_body);

    json_decref(json_users);
    json_decref(json_body);
    db_users_free(users, user_count);

    log_request(request, response, start);
    return U_CALLBACK_CONTINUE;
}

/**
 * callback_get_user_by_id
 */
static int
callback_get_user_by_id(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    clock_t start = clock();

    const char *auth_token = u_map_get(request->map_header, AUTH_HEADER);

    user_t *user1 = db_user_new();
    int row_count = db_user_get_by_token(dbr, auth_token, user1);
    if (strcmp(user1->username, "admin")) {
        ulfius_set_string_body_response(response, HTTP_STATUS_UNAUTHORIZED, "error authentication");
        return U_CALLBACK_UNAUTHORIZED;
    }
    db_user_free(user1);

    char *idv = u_map_get(request->map_url, "id");
    char *endptr;
    long id = strtol(idv, &endptr, 10);

    user_t *user = db_user_new();
    uint64_t user_count = db_user_get_by_id(dbr, id, user);
    if (user_count == 0) {
        ulfius_set_string_body_response(response, HTTP_STATUS_NOT_FOUND, ULFIUS_HTTP_NOT_FOUND_BODY);
        return U_CALLBACK_CONTINUE;
    }

    json_t *json_body = json_object();
    json_body = json_pack("{s:i, s:s, s:s}", "id", user->id, "first_name", user->first_name, "last_name", user->last_name);

    ulfius_set_json_body_response(response, HTTP_STATUS_OK, json_body);

    json_decref(json_body);
    db_user_free(user);

    log_request(request, response, start);
    return U_CALLBACK_CONTINUE;
}

/// @brief retrieves a password for the given name
/// @param request 
/// @param response 
/// @param user_data 
/// @return 
static int
callback_get_password(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    clock_t start = clock();

    char *p_name = u_map_get(request->map_url, "name");
    const char *token = u_map_get(request->map_header, AUTH_HEADER);

    password_t *pass = db_password_new();
    if (db_password_get_by_token(dbr, p_name, token, pass) != 0) {
        // handle the error here
    }

    json_t *json_body = json_object();
    json_body = json_pack("{s:i, s:s, s:s}", "id", pass->id, "name", pass->name, "password", pass->password);

    ulfius_set_json_body_response(response, HTTP_STATUS_OK, json_body);

    json_decref(json_body);
    db_password_free(pass);

    log_request(request, response, start);
    return U_CALLBACK_CONTINUE;
}


/// @brief adds a password for a given user
/// @param request 
/// @param response 
/// @param user_data 
/// @return 
static int
callback_new_password(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    clock_t start = clock();

    const char *token = u_map_get(request->map_header, AUTH_HEADER);
    
    json_error_t error;
    json_t *json_new_user_request = ulfius_get_json_body_request(request, &error);
    const char *name = json_string_value(json_object_get(json_new_user_request, "name"));
    const char *username = json_string_value(json_object_get(json_new_user_request, "username"));
    const char *password = json_string_value(json_object_get(json_new_user_request, "password"));
    if (strcmp(error.text, "")) {
        printf("error: %s", error.text);
        ulfius_set_string_body_response(response, HTTP_STATUS_BAD_REQUEST, "");
    }
    
    user_t *user = db_user_new();
    int row_count = db_user_get_by_token(dbr, token, user);
    if (row_count == 0) {
        // token not found
        return U_CALLBACK_UNAUTHORIZED;
    }

    if (db_password_add(dbr, name, username, password, "", user->id) != 0) {
        printf("error: %s\n", db_get_error(dbr));
        ulfius_set_string_body_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "failed to add new password");
        return U_CALLBACK_ERROR;
    }

    ulfius_set_string_body_response(response, HTTP_STATUS_CREATED, "");

    json_decref(json_new_user_request);
    db_user_free(user);

    log_request(request, response, start);
    return U_CALLBACK_CONTINUE;
}

int
api_init(db_t *db)
{
    dbr = db;

    if (ulfius_init_instance(&instance, DEFAULT_PORT, NULL, NULL) != U_OK) {
        fprintf(stderr, "error ulfius_init_instance, abort\n");
        return EXIT_FAILURE;
    }

    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_GET, HEALTH_PATH, NULL, 0, &callback_health, NULL);

    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_POST, API_PATH, USER_PATH, 0, &callback_new_user, NULL);
    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_GET, API_PATH, USERS_PATH, 0, &callback_get_users, NULL);
    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_GET, API_PATH, USER_BY_ID_PATH, 0, &callback_get_user_by_id, NULL);

    //ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_POST, API_PATH, PASSWORD_PATH, 0, &callback_auth_token, NULL);
    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_POST, API_PATH, PASSWORD_PATH, 0, &callback_new_password, NULL);
    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_GET, API_PATH, PASSWORD_BY_NAME_PATH, 0, &callback_get_password, NULL);

    ulfius_set_default_endpoint(&instance, &callback_default, NULL);

    return 0;
}

void
api_start()
{
    if (ulfius_start_framework(&instance) == U_OK) {
        s_log(LOG_INFO, s_log_string("msg", "initializing database"), s_log_int("port", instance.port));

        getchar();
    } else {
        s_log(LOG_ERROR, s_log_string("msg", "error starting server"));
    }

    ulfius_stop_framework(&instance);
    ulfius_clean_instance(&instance);
}

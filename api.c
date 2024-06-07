#include <arpa/inet.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>

#include <jansson.h>
#include <orcania.h>
#include <ulfius.h>

#include "base64.h"
#include "database.h"
#include "http.h"
#include "logger.h"
#include "pass.h"

#define DEFAULT_PORT 8080

#define AUTH_HEADER "X-Hush-Auth"

#define LOGIN_PATH "/login"
#define HEALTH_PATH  "/healthz"
#define API_PATH "/api/v1"
#define USER_PATH "/user"
#define USERS_PATH "/users"
#define USER_BY_ID_PATH USER_PATH "/:id"
#define USER_KEY_PATH "/user/key/:username"
#define PASSWORD_PATH "/password"
#define PASSWORDS_PATH "/passwords"
#define PASSWORD_BY_NAME_PATH PASSWORD_PATH "/:name"
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
        s_log_int("duration", msec),
        s_log_string("client_addr", inet_ntoa(((struct sockaddr_in*)request->client_address)->sin_addr)));
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
        s_log(LOG_ERROR, s_log_string("msg", error.text));
        response->status = HTTP_STATUS_BAD_REQUEST;
        ulfius_set_string_body_response(response, HTTP_STATUS_BAD_REQUEST, "");
        return U_CALLBACK_ERROR;
    }

    char *token = generate_password(32);

    if (db_user_add(dbr, username, first_name, last_name, password, token) != 0) {
        s_log(LOG_ERROR, s_log_string("msg", db_get_error(dbr)));
        ulfius_set_string_body_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "failed to add new user");
        return U_CALLBACK_ERROR;
    }

    user_t *user = db_user_new();
    if (db_user_get_by_username(dbr, username, user) != 1) {
        s_log(LOG_ERROR, s_log_string("msg", db_get_error(dbr)));
        response->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
        ulfius_set_string_body_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "failed to add new user");
        return U_CALLBACK_ERROR;
    }
    
    unsigned char key[crypto_secretbox_KEYBYTES];
    crypto_secretbox_keygen(key);
    if (db_key_add(dbr, key, user->id) != 0) {
        s_log(LOG_ERROR, s_log_string("msg", db_get_error(dbr)));
        response->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
        ulfius_set_string_body_response(response, HTTP_STATUS_INTERNAL_SERVER_ERROR, "failed to add new user");
        return U_CALLBACK_ERROR;
    }

    json_t *json_body = json_object();
    json_body = json_pack("{s:s}", "token", token);
    ulfius_set_json_body_response(response, HTTP_STATUS_OK, json_body);

    json_decref(json_new_user_request);
    json_decref(json_body);
    free((char *)token);
    
    log_request(request, response, start);
    return U_CALLBACK_CONTINUE;
}

/**
 * callback_get_users
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
 * callback_get_user_key
 */
static int
callback_get_user_key(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    clock_t start = clock();

    const char *username = u_map_get(request->map_url, "username");
    const char *token = u_map_get(request->map_header, AUTH_HEADER);

    user_t *user = db_user_new();
    int row_count = db_user_get_by_token(dbr, token, user);
    if (row_count == 0) {
        // token not found
        return U_CALLBACK_UNAUTHORIZED;
    }
    
    u_key_t *key = db_key_new();
    if (db_key_get_by_user_id(dbr, user->id, key) != 1) {
        // key not found error
        
    }

    char *encoded_key = base64_encode((const unsigned char *)key->key, 32);
    json_t *json_body = json_object();
    json_body = json_pack("{s:s}", "key", encoded_key);

    ulfius_set_json_body_response(response, HTTP_STATUS_OK, json_body);

    json_decref(json_body);
    db_key_free(key);

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

    const char *idv = u_map_get(request->map_url, "id");
    char *endptr;
    long id = strtol(idv, &endptr, 10);

    user_t *user = db_user_new();
    uint64_t user_count = db_user_get_by_id(dbr, id, user);
    if (user_count == 0) {
        ulfius_set_string_body_response(response, HTTP_STATUS_NOT_FOUND, ULFIUS_HTTP_NOT_FOUND_BODY);
        log_request(request, response, start);
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

static int
callback_get_password(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    clock_t start = clock();

    const char *p_name = u_map_get(request->map_url, "name");
    const char *token = u_map_get(request->map_header, AUTH_HEADER);

    password_t *pass = db_password_new();
    if (db_password_get_by_token(dbr, p_name, token, pass) == 0) {
        // handle the error here
    }

    json_t *json_body = json_object();
    json_body = json_pack("{s:i, s:s, s:s, s:s}", "id", pass->id, "name", pass->name, "username", pass->username, "password", pass->password);

    ulfius_set_json_body_response(response, HTTP_STATUS_OK, json_body);

    json_decref(json_body);
    db_password_free(pass);

    log_request(request, response, start);
    return U_CALLBACK_CONTINUE;
}

static int
callback_get_passwords(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    clock_t start = clock();

    const char *token = u_map_get(request->map_header, AUTH_HEADER);

    password_t **passwords = db_passwords_new();
    int password_count = db_passwords_get_by_token(dbr, token, passwords);
    if (password_count == 0) {
        ulfius_set_string_body_response(response, HTTP_STATUS_NOT_FOUND, ULFIUS_HTTP_NOT_FOUND_BODY);
        log_request(request, response, start);
        return U_CALLBACK_CONTINUE;
    }

    json_t *json_passwords = json_array();
    for (int i = 0; i < password_count; i++) {
        if (passwords[i] != NULL) {
            // if (passwords[i]->name == NULL) printf("XXX - name");
            // if (passwords[i]->username == NULL) printf("XXX - username");
            // if (passwords[i]->password == NULL) printf("XXX - password");
            // printf("XXX - %lu, %s, %s, %s\n", passwords[i]->id, passwords[i]->name, passwords[i]->username, passwords[i]->password);
            json_t *jp = json_pack("{s:i, s:s, s:s, s:s}",
                "id", passwords[i]->id,
                "name", passwords[i]->name,
                "username", passwords[i]->username,
                "password", passwords[i]->password);
            json_array_append_new(json_passwords, jp);
        }
    }

    json_t *json_body = json_object();
    json_body = json_pack("{s:i, s:O}", "count", password_count, "passwords", json_passwords);
    ulfius_set_json_body_response(response, HTTP_STATUS_OK, json_body);

    json_decref(json_body);
    db_passwords_free(passwords, password_count);

    log_request(request, response, start);
    return U_CALLBACK_CONTINUE;
}

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
        s_log(LOG_ERROR, s_log_string("msg", error.text));
        ulfius_set_string_body_response(response, HTTP_STATUS_BAD_REQUEST, "");
        return U_CALLBACK_ERROR;
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

static int
callback_login(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    clock_t start = clock();
    
    json_error_t error;
    json_t *json_new_user_request = ulfius_get_json_body_request(request, &error);
    const char *username = json_string_value(json_object_get(json_new_user_request, "username"));
    const char *password = json_string_value(json_object_get(json_new_user_request, "password"));
    if (strcmp(error.text, "") != 0) {
        s_log(LOG_ERROR, s_log_string("msg", error.text));
        ulfius_set_string_body_response(response, HTTP_STATUS_BAD_REQUEST, "");
        log_request(request, response, start);
        return U_CALLBACK_ERROR;
    }

    user_t *user = db_user_new();
    if (db_user_get_token(dbr, username, password, user) == 0) {
        response->status = HTTP_STATUS_UNAUTHORIZED;
        log_request(request, response, start);
        return U_CALLBACK_UNAUTHORIZED;
    }
    
    json_t *json_body = json_object();
    json_body = json_pack("{s:s}", "token", user->token);

    ulfius_set_json_body_response(response, HTTP_STATUS_OK, json_body);

    json_decref(json_new_user_request);
    json_decref(json_body);
    db_user_free(user);

    log_request(request, response, start);
    return U_CALLBACK_CONTINUE;
}

#define STATIC_FILE_CHUNK 256

struct static_file_config {
    char *files_path;
    char *url_prefix;
    struct _u_map *mime_types;
    struct _u_map *map_header;
    char *redirect_on_404;
};

/**
 * get_filename_ext returns the filename extension.
 */
const char*
get_filename_ext(const char *path)
{
    const char *dot = strrchr(path, '.');

    if (!dot || dot == path) {
        return "*";
    }

    if (strchr(dot, '?') != NULL) {
        *strchr(dot, '?') = '\0';
    }

    return dot;
}

/**
 * callback_static_file_stream callback function to ease sending large files.
 */
static ssize_t
callback_static_file_stream(void *cls, uint64_t pos, char *buf, size_t max)
{
    (void)(pos);
    if (cls != NULL) {
        return fread(buf, 1, max, (FILE *)cls);
    }

    return U_STREAM_END;
}

/**
 * Cleanup FILE* structure when streaming is complete
 */
static void
callback_static_file_stream_free(void * cls)
{
    if (cls != NULL) {
        fclose((FILE *)cls);
    }
}

int
callback_static_file(const struct _u_request *request, struct _u_response *response, void *user_data)
{
    size_t length;
    FILE * f;
    char * file_requested, * file_path, * url_dup_save, * real_path = NULL;
    const char * content_type;

    /*
    * Comment this if statement if you don't access static files url from root dir, like /app
    */
    if (request->callback_position > 0) {
        return U_CALLBACK_CONTINUE;
    } else if (user_data != NULL && ((struct static_file_config *)user_data)->files_path != NULL) {
        file_requested = strdup(request->http_url);
        url_dup_save = file_requested;
    
        while (file_requested[0] == '/') {
            file_requested++;
        }
        file_requested += strlen(((struct static_file_config *)user_data)->url_prefix);

        while (file_requested[0] == '/') {
            file_requested++;
        }

        if (strchr(file_requested, '#') != NULL) {
            *strchr(file_requested, '#') = '\0';
        }

        if (strchr(file_requested, '?') != NULL) {
            *strchr(file_requested, '?') = '\0';
        }
    
    if (file_requested == NULL || strlen(file_requested) == 0 || 0 == strcmp("/", file_requested)) {
        free(url_dup_save);
        url_dup_save = file_requested = strdup("index.html");
    }
    
    file_path = msprintf("%s/%s", ((struct static_file_config *)user_data)->files_path, file_requested);
    real_path = realpath(file_path, NULL);
    if (0 == strncmp(((struct static_file_config *)user_data)->files_path, real_path, strlen(((struct static_file_config *)user_data)->files_path))) {
        if (access(file_path, F_OK) != -1) {
        f = fopen (file_path, "rb");
        if (f) {
            fseek (f, 0, SEEK_END);
            length = ftell (f);
            fseek (f, 0, SEEK_SET);
          
            content_type = u_map_get_case(((struct static_file_config *)user_data)->mime_types, get_filename_ext(file_requested));
            if (content_type == NULL) {
                content_type = u_map_get(((struct static_file_config *)user_data)->mime_types, "*");
                s_log(LOG_WARN, s_log_string("msg", "Unknown mime type for extension"),
                    s_log_string("extension", get_filename_ext(file_requested)));
            }
            u_map_put(response->map_header, "Content-Type", content_type);
            u_map_copy_into(response->map_header, ((struct static_file_config *)user_data)->map_header);
            
            if (ulfius_set_stream_response(response, 200, callback_static_file_stream, callback_static_file_stream_free, length, STATIC_FILE_CHUNK, f) != U_OK) {
                s_log(LOG_ERROR, s_log_string("msg", "error ulfius_set_stream_response"));
            }
            }
            } else {
                if (((struct static_file_config *)user_data)->redirect_on_404 == NULL) {
                    ulfius_set_string_body_response(response, 404, "File not found");
                } else {
                    ulfius_add_header_to_response(response, "Location", ((struct static_file_config *)user_data)->redirect_on_404);
                    response->status = 302;
                }
            }
            } else {
                if (((struct static_file_config *)user_data)->redirect_on_404 == NULL) {
                    ulfius_set_string_body_response(response, 404, "file not found");
                } else {
                    ulfius_add_header_to_response(response, "Location", ((struct static_file_config *)user_data)->redirect_on_404);
                    response->status = 302;
                }
            }

            free(file_path);
            free(url_dup_save);
            free(real_path); // realpath uses malloc

            return U_CALLBACK_CONTINUE;
    } else {
        s_log(LOG_ERROR, s_log_string("msg", "user_data is NULL or inconsistent"));
        return U_CALLBACK_ERROR;
    }
}

int
api_init(db_t *db)
{
    dbr = db;

    if (ulfius_init_instance(&instance, DEFAULT_PORT, NULL, NULL) != U_OK) {
        fprintf(stderr, "error ulfius_init_instance, abort\n");
        return EXIT_FAILURE;
    }

    struct static_file_config config;

    // Add mime types
    u_map_put(config.mime_types, "*", "application/octet-stream");
    u_map_put(config.mime_types, ".html", "text/html");
    u_map_put(config.mime_types, ".css", "text/css");
    u_map_put(config.mime_types, ".js", "application/javascript");
    u_map_put(config.mime_types, ".json", "application/json");
    u_map_put(config.mime_types, ".png", "image/png");
    u_map_put(config.mime_types, ".gif", "image/gif");
    u_map_put(config.mime_types, ".jpeg", "image/jpeg");
    u_map_put(config.mime_types, ".jpg", "image/jpeg");
    u_map_put(config.mime_types, ".ttf", "font/ttf");
    u_map_put(config.mime_types, ".woff", "font/woff");
    u_map_put(config.mime_types, ".ico", "image/x-icon");

    // ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_GET, NULL, "*", 0, &callback_static_file, &config);

    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_GET, HEALTH_PATH, NULL, 0, &callback_health, NULL);

    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_POST, LOGIN_PATH, NULL, 0, &callback_login, NULL);

    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_POST, API_PATH, USER_PATH, 0, &callback_new_user, NULL);
    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_GET, API_PATH, USERS_PATH, 0, &callback_get_users, NULL);
    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_GET, API_PATH, USER_BY_ID_PATH, 0, &callback_get_user_by_id, NULL);
    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_GET, API_PATH, USER_KEY_PATH, 0, &callback_get_user_key, NULL);

    //ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_POST, API_PATH, PASSWORD_PATH, 0, &callback_auth_token, NULL);
    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_POST, API_PATH, PASSWORD_PATH, 0, &callback_new_password, NULL);
    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_GET, API_PATH, PASSWORD_BY_NAME_PATH, 0, &callback_get_password, NULL);
    ulfius_add_endpoint_by_val(&instance, HTTP_METHOD_GET, API_PATH, PASSWORDS_PATH, 0, &callback_get_passwords, NULL);

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

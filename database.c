#include <stdbool.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"
#include "pass.h"

#define CREATE_TABLE_USERS_QUERY "CREATE TABLE IF NOT EXISTS users (" \
    "id int NOT NULL AUTO_INCREMENT," \
    "username varchar(255) NOT NULL," \
    "first_name varchar(255)," \
    "last_name varchar(255)," \
    "password varchar(255)," \
    "token varchar(255)," \
    "PRIMARY KEY (ID)," \
    "UNIQUE (username)" \
");"

#define CREATE_TABLE_PASSWORDS_QUERY "CREATE TABLE IF NOT EXISTS passwords (" \
    "id int NOT NULL AUTO_INCREMENT," \
    "name varchar(255) NOT NULL," \
    "username varchar(255) NOT NULL," \
    "password text NOT NULL," \
    "user_id int  NOT NULL," \
    "PRIMARY KEY (id)," \
    "FOREIGN KEY (user_id) REFERENCES users(id)," \
    "UNIQUE name_user_id_idx (name, user_id)" \
");"

#define CREATE_TABLE_KEYS_QUERY "create table if not exists `keys` (" \
    "id int NOT NULL AUTO_INCREMENT," \
    "`key` text NOT NULL," \
    "user_id int NOT NULL," \
    "PRIMARY KEY (id)," \
    "FOREIGN KEY (user_id) REFERENCES users(id)" \
");"

#define CREATE_TABLE_LABELS_QUERY "CREATE TABLE IF NOT EXISTS labels (" \
    "id int NOT NULL AUTO_INCREMENT," \
    "name varchar(255) NOT NULL," \
    "PRIMARY KEY (id)," \
    "UNIQUE (name)" \
");"

#define CREATE_TABLE_PASSWORD_LABELS_QUERY "CREATE TABLE IF NOT EXISTS password_labels (" \
    "id int NOT NULL AUTO_INCREMENT," \
    "label_id int NOT NULL," \
    "password_id int NOT NULL," \
    "PRIMARY KEY (id)," \
    "FOREIGN KEY (label_id) REFERENCES labels(id)," \
    "FOREIGN KEY (password_id) REFERENCES passwords(id)" \
");"

#define INSERT_PASSWORD_QUERY "INSERT INTO passwords (name, username, password, user_id) VALUES (?, ?, ?, ?)"
#define INSERT_USER_QUERY "INSERT INTO users (username, first_name, last_name, password, token) VALUES (?, ?, ?, PASSWORD(?), ?)"
#define INSERT_USER_KEY_QUERY "INSERT INTO `keys` (`key`, user_id) VALUES (?, ?)"
#define SELECT_ALL_USERS_QUERY "SELECT * FROM users"
#define SELECT_USER_BY_NAME_QUERY "SELECT * FROM users WHERE username = '%s'"
#define SELECT_USER_BY_TOKEN_QUERY "SELECT * FROM users WHERE token = '%s'"
#define SELECT_USER_BY_ID_QUERY "SELECT * FROM users WHERE id = %lu"
#define SELECT_USER_IS_ADMIN "SELECT id FROM users WHERE token = '%s'"
#define SELECT_PASSWORD_BY_NAME_QUERY "SELECT * FROM passwords WHERE name = '%s' AND user_id = %lu"
#define SELECT_PASSWORD_BY_TOKEN_QUERY "SELECT * FROM passwords WHERE name = '%s' AND user_id = (SELECT id FROM users WHERE token = '%s')"
#define SELECT_PASSWORDS_BY_TOKEN_QUERY "SELECT p.id, p.name, p.username, p.password FROM passwords AS p JOIN users AS u ON p.user_id = u.id WHERE u.token = '%s'"
#define SELECT_TOKEN_BY_USERNAME_QUERY "SELECT token FROM users WHERE username = '%s' AND password = PASSWORD('%s')"
#define SELECT_KEY_BY_USER_ID_QUERY "SELECT CONVERT(`key` USING utf8) FROM `keys` WHERE user_id = %lu"


MYSQL_ROW row;

struct db {
    MYSQL *conn;
};

db_t*
db_new()
{
    return (struct db *)malloc(sizeof(struct db));
}

int
db_init(db_t *db, const char *server, const char *user, const char *password, const char *database)
{
    db->conn = mysql_init(NULL);

    if (!mysql_real_connect(db->conn, server, user, password, database, 0, NULL, 0)) {
        return 1;
    }

    if (mysql_query(db->conn, CREATE_TABLE_USERS_QUERY)) {
        return 2;
    }

    if (mysql_query(db->conn, CREATE_TABLE_PASSWORDS_QUERY)) {
        return 3;
    }

    if (mysql_query(db->conn, CREATE_TABLE_KEYS_QUERY)) {
        return 4;
    }

    if (mysql_query(db->conn, CREATE_TABLE_LABELS_QUERY)) {
        return 5;
    }

    if (mysql_query(db->conn, CREATE_TABLE_PASSWORD_LABELS_QUERY)) {
        return 6;
    }

    const char *token = generate_password(32);
    db_user_add(db, getenv("ADMIN_USERNAME"), getenv("ADMIN_FIRST_NAME"), getenv("ADMIN_LAST_NAME"), getenv("ADMIN_PASSWORD"), token);
    free((char *)token);

    return 0;
}

void
db_cleanup(db_t *db)
{
    if (db == NULL) {
        return;
    }

    if (db->conn != NULL) {
        mysql_close(db->conn);
    }
    
    free(db);
}

const char*
db_get_error(db_t *db)
{
    return mysql_error(db->conn);
}

user_t*
db_user_new()
{
    user_t *user = malloc(sizeof(user_t));
    user->id = 0;
    user->first_name = calloc(0, sizeof(char));
    user->last_name = calloc(0, sizeof(char));
    user->password = calloc(0, sizeof(char));
    user->token = calloc(0, sizeof(char));

    return user;
}

user_t**
db_users_new()
{
    user_t **users = malloc(sizeof(user_t)*2);

    for (int i = 0; i < 2; i++) {
        users[i] = db_user_new();
    }

    return users;
}

password_t*
db_password_new()
{
    password_t *pass = malloc(sizeof(user_t));
    pass->id = 0;
    pass->name = calloc(0, sizeof(char));
    pass->username = calloc(0, sizeof(char));
    pass->password = calloc(0, sizeof(char));
    pass->user_id = 0;

    return pass;
}

password_t**
db_passwords_new()
{
    password_t **passwords = malloc(sizeof(password_t)*2);

    for (int i = 0; i < 2; i++) {
        passwords[i] = db_password_new();
    }

    return passwords;
}

int
db_password_add(db_t *db, const char *name, const char *username, const char *password, const char *labels, const long user_id)
{
    MYSQL_STMT *insert_password_stmt = mysql_stmt_init(db->conn);

    int result = mysql_stmt_prepare(insert_password_stmt, INSERT_PASSWORD_QUERY, strlen(INSERT_PASSWORD_QUERY));  
    if (result != 0) {
        return result;
    }

    MYSQL_BIND bind[4];

    unsigned int array_size = 1; 
    unsigned long name_len = strlen(name);
    unsigned long username_len = strlen(username);
    unsigned long password_len = strlen(password);

    memset(bind, 0, sizeof(bind)); 

    bind[0].buffer_type = MYSQL_TYPE_STRING; 
    bind[0].buffer = (char *)name;
    bind[0].buffer_length = strlen(name); 
    bind[0].length = &name_len;

    bind[1].buffer_type = MYSQL_TYPE_STRING; 
    bind[1].buffer = (char *)username;
    bind[1].buffer_length = strlen(username); 
    bind[1].length = &username_len;

    bind[2].buffer_type = MYSQL_TYPE_STRING; 
    bind[2].buffer = (char *)password;
    bind[2].buffer_length = strlen(password); 
    bind[2].length = &password_len;

    bind[3].buffer_type = MYSQL_TYPE_LONG; 
    bind[3].buffer = (long*)&user_id; 
    bind[3].is_null = 0;
    bind[3].length = 0;

    if (mysql_stmt_bind_param(insert_password_stmt, bind)) {
        return result;
    }

    MYSQL_RES *res;
    result = mysql_stmt_execute(insert_password_stmt); 
    if (result != 0) {
        return result;
    }

    mysql_free_result(res);

    return 0;
}

int
db_label_add(db_t *db, char *label, long pass_id)
{
    return 0;
}

int
db_user_add(db_t *db, const char *username, const char *first_name, const char *last_name, const char *password, const char *token)
{
    MYSQL_STMT *insert_user_stmt = mysql_stmt_init(db->conn);

    int result = mysql_stmt_prepare(insert_user_stmt, INSERT_USER_QUERY, strlen(INSERT_USER_QUERY));  
    if (result != 0) {
        return result;
    }
    
    MYSQL_BIND bind[5];
    memset(bind, 0, sizeof(bind));

    unsigned int array_size = 1; 
    unsigned long username_len = strlen(username);
    unsigned long first_name_len = strlen(first_name);
    unsigned long last_name_len = strlen(last_name);
    unsigned long password_len = strlen(password);
    unsigned long token_len = strlen(token);

    bind[0].buffer_type = MYSQL_TYPE_STRING; 
    bind[0].buffer = (char *)username;
    bind[0].buffer_length = strlen(username); 
    bind[0].length = &username_len;

    bind[1].buffer_type = MYSQL_TYPE_STRING; 
    bind[1].buffer = (char *)first_name;
    bind[1].buffer_length = strlen(first_name); 
    bind[1].length = &first_name_len;

    bind[2].buffer_type = MYSQL_TYPE_STRING; 
    bind[2].buffer = (char *)last_name;
    bind[2].buffer_length = strlen(last_name); 
    bind[2].length = &last_name_len;

    bind[3].buffer_type = MYSQL_TYPE_STRING; 
    bind[3].buffer = (char *)password;
    bind[3].buffer_length = strlen(password); 
    bind[3].length = &password_len;

    bind[4].buffer_type = MYSQL_TYPE_STRING; 
    bind[4].buffer = (char *)token;
    bind[4].buffer_length = strlen(token); 
    bind[4].length = &token_len;
    
    if (mysql_stmt_bind_param(insert_user_stmt, bind)) {
        return result;
    }
    
    result = mysql_stmt_execute(insert_user_stmt); 
    if (result != 0) {
        return result;
    }

    return 0;
}

uint64_t
db_users_get_all(db_t *db, user_t **users)
{
    if (mysql_query(db->conn, SELECT_ALL_USERS_QUERY)) {
        return -1;
    }

    MYSQL_RES *res = mysql_store_result(db->conn);
    uint64_t row_count = mysql_num_rows(res);
    if (row_count == 0) {
        goto CLEANUP;
    }

    if (row_count > 2) {
        users = realloc(users, sizeof(user_t)*row_count);
    }
    
    uint64_t i = 0;
    char *endptr;

    while ((row = mysql_fetch_row(res)) != NULL) {
        user_t *user = malloc(sizeof(user_t));

        user->id = strtol(row[0], &endptr, 10);
        user->username = realloc(user->username, strlen(row[1]));
        strcpy(user->username, row[1]);

        user->first_name = realloc(user->first_name, strlen(row[2]));
        strcpy(user->first_name, row[2]);

        user->last_name = realloc(user->last_name, strlen(row[3]));
        strcpy(user->last_name, row[3]);

        user->password = realloc(user->password, strlen(row[4]));
        strcpy(user->password, row[4]);

        user->token = realloc(user->token, strlen(row[4]));
        strcpy(user->password, row[4]);

        users[i] = user;
        i++;
    }

CLEANUP:
    mysql_free_result(res);

    return row_count;
}

int
db_user_get_by_username(db_t *db, const char *username, user_t *user)
{
    char *query = malloc(strlen(SELECT_USER_BY_NAME_QUERY)+strlen(username));
    sprintf(query, SELECT_USER_BY_NAME_QUERY, username);

    if (mysql_query(db->conn, query)) {
        return -1;
    }

    MYSQL_RES *res = mysql_store_result(db->conn);
    uint64_t row_count = mysql_num_rows(res);
    if (row_count == 0) {
        goto CLEANUP;
    }

    char *endptr;

    while ((row = mysql_fetch_row(res)) != NULL) {
        user->id = strtol(row[0], &endptr, 10);
        user->username = realloc(user->username, strlen(row[1]));
        strcpy(user->username, row[1]);

        user->first_name = realloc(user->first_name, strlen(row[2]));
        strcpy(user->first_name, row[2]);

        user->last_name = realloc(user->last_name, strlen(row[3]));
        strcpy(user->last_name, row[3]);

        user->password = realloc(user->password, strlen(row[4]));
        strcpy(user->password, row[4]);

        user->token = realloc(user->token, strlen(row[5]));
        strcpy(user->token, row[5]);
    }

CLEANUP:
    free(query);
    mysql_free_result(res);

    return row_count;
}

int
db_user_get_by_id(db_t *db, const long id, user_t *user)
{
    char sid[10 + sizeof(char)];
    sprintf(sid, "%ld", id);

    char *query = malloc(strlen(SELECT_USER_BY_ID_QUERY)+strlen(sid));
    sprintf(query, SELECT_USER_BY_ID_QUERY, id);

    if (mysql_query(db->conn, query)) {
        return -1;
    }

    MYSQL_RES *res = mysql_store_result(db->conn);
    uint64_t row_count = mysql_num_rows(res);
    if (row_count == 0) {
        goto CLEANUP;
    }

    char *endptr;

    while ((row = mysql_fetch_row(res)) != NULL) {
        user->id = strtol(row[0], &endptr, 10);
        user->username = malloc(strlen(row[1]));
        strcpy(user->username, row[1]);

        user->first_name = malloc(strlen(row[2]));
        strcpy(user->first_name, row[2]);

        user->last_name = malloc(strlen(row[3]));
        strcpy(user->last_name, row[3]);

        user->password = malloc(strlen(row[4]));
        strcpy(user->password, row[4]);

        user->token = malloc(strlen(row[5]));
        strcpy(user->token, row[5]);
    }

CLEANUP:
    free(query);
    mysql_free_result(res);

    return row_count;
}

int
db_user_get_by_token(db_t *db, const char *token, user_t *user)
{
    char *query = malloc(strlen(SELECT_USER_BY_TOKEN_QUERY)+strlen(token));
    sprintf(query, SELECT_USER_BY_TOKEN_QUERY, token);

    if (mysql_query(db->conn, query)) {
        return -1;
    }

    MYSQL_RES *res = mysql_store_result(db->conn);
    uint64_t row_count = mysql_num_rows(res);
    if (row_count == 0) {
        goto CLEANUP;
    }

    char *endptr;

    while ((row = mysql_fetch_row(res)) != NULL) {
        user->id = strtol(row[0], &endptr, 10);

        user->username = realloc(user->token, strlen(row[1]));
        strcpy(user->username, row[1]);

        user->first_name = realloc(user->token, strlen(row[2]));
        strcpy(user->first_name, row[2]);

        user->last_name = realloc(user->token, strlen(row[3]));
        strcpy(user->last_name, row[3]);

        user->password = realloc(user->token, strlen(row[4]));
        strcpy(user->password, row[4]);

        user->token = realloc(user->token,strlen(row[5]));
        strcpy(user->token, row[5]);
    }

CLEANUP:
    free(query);
    mysql_free_result(res);

    return row_count;
}

int
db_user_get_token(db_t *db, const char *username, const char *password, user_t *user)
{
    char *query = malloc(strlen(SELECT_TOKEN_BY_USERNAME_QUERY)+strlen(username)+strlen(password));
    sprintf(query, SELECT_TOKEN_BY_USERNAME_QUERY, username, password);

    if (mysql_query(db->conn, query)) {
        return 0;
    }
    
    MYSQL_RES *res = mysql_store_result(db->conn);
    uint64_t row_count = mysql_num_rows(res);

    if (row_count == 0) {
        return row_count;
    }

    while ((row = mysql_fetch_row(res)) != NULL) {
        user->token = realloc(user->token, strlen(row[0]));
        strcpy(user->token, row[0]);
    }

    free(query);
    mysql_free_result(res);

    return row_count;
}

// db_user_free frees the memory allocated for the 
// get user call.
void
db_user_free(user_t *user)
{
    if (user == NULL) {
        return;
    }

    if (user->username != NULL) {
        free(user->username);
    }

    if (user->first_name != NULL) {
        free(user->first_name);
    }

    if (user->last_name != NULL) {
        free(user->last_name);
    }

    if (user->password != NULL) {
        free(user->password);
    }
    
    if (user->token != NULL) {
        free(user->token);
    }

    free(user);
}

// db_users_free frees the memory allocated for the 
// get users call.
void
db_users_free(user_t **user, const uint64_t size)
{
    if (user == NULL) {
        return;
    }

    for (uint64_t i = 0; i < size; i++) {
        if (user[i]->username != NULL) {
            free(user[i]->username);
        }

        if (user[i]->first_name != NULL) {
            free(user[i]->first_name);
        }

        if (user[i]->last_name != NULL) {
            free(user[i]->last_name);
        }

        if (user[i]->password != NULL) {
            free(user[i]->password);
        }

        free(user[i]);
    }
}

int
db_password_get_by_name(db_t *db, const char *name, const long user_id, password_t *pass)
{
    char *query = malloc(strlen(SELECT_PASSWORD_BY_NAME_QUERY)+strlen(name));
    sprintf(query, SELECT_PASSWORD_BY_NAME_QUERY, name, user_id);

    if (mysql_query(db->conn, query)) {
        return 1;
    }

    MYSQL_RES *res = mysql_store_result(db->conn);
    uint64_t row_count = mysql_num_rows(res);
    if (row_count == 0) {
        goto CLEANUP;
    }

    char *endptr;

    while ((row = mysql_fetch_row(res)) != NULL) {
        pass->id = strtol(row[0], &endptr, 10);

        pass->name = malloc(strlen(row[1]));
        strcpy(pass->name, row[1]);

        pass->username = malloc(strlen(row[1]));
        strcpy(pass->username, row[1]);

        pass->password = malloc(strlen(row[2]));
        strcpy(pass->password, row[2]);

        pass->user_id = strtol(row[3], &endptr, 10);
    }

CLEANUP:
    free(query);
    mysql_free_result(res);

    return 0;
}

int
db_password_get_by_token(db_t *db, const char *name, const char *token, password_t *pass)
{
    char *query = malloc(strlen(SELECT_PASSWORD_BY_TOKEN_QUERY)+strlen(name)+strlen(token));
    sprintf(query, SELECT_PASSWORD_BY_TOKEN_QUERY, name, token);

    if (mysql_query(db->conn, query)) {
        return 1;
    }

    MYSQL_RES *res = mysql_store_result(db->conn);
    uint64_t row_count = mysql_num_rows(res);

    char *endptr;

    while ((row = mysql_fetch_row(res)) != NULL) {
        pass->id = strtol(row[0], &endptr, 10);

        pass->name = malloc(strlen(row[1]));
        strcpy(pass->name, row[1]);

        pass->username = malloc(strlen(row[2]));
        strcpy(pass->username, row[2]);

        pass->password = malloc(strlen(row[3]));
        strcpy(pass->password, row[3]);
    }

    free(query);
    mysql_free_result(res);

    return 0;
}

int
db_passwords_get_by_token(db_t *db, const char *token, password_t **passwords)
{
    char *query = malloc(strlen(SELECT_PASSWORDS_BY_TOKEN_QUERY)+strlen(token)+1);
    sprintf(query, SELECT_PASSWORDS_BY_TOKEN_QUERY, token);

    if (mysql_query(db->conn, query)) {
        return 1;
    }

    MYSQL_RES *res = mysql_store_result(db->conn);
    uint64_t row_count = mysql_num_rows(res);
    if (row_count == 0) {
        goto CLEANUP;
    }

    if (row_count > 2) {
        passwords = realloc(passwords, sizeof(password_t*)*row_count);
    }
    
    uint64_t i = 0;
    char *endptr;

    while ((row = mysql_fetch_row(res)) != NULL) {
        password_t *pass = db_password_new();

        pass->id = strtol(row[0], &endptr, 10);

        pass->name = realloc(pass->name, strlen(row[1])+1);
        strcpy(pass->name, row[1]);

        pass->username = realloc(pass->username, strlen(row[2])+1);
        strcpy(pass->username, row[2]);

        pass->password = realloc(pass->password, strlen(row[3])+1);
        strcpy(pass->password, row[3]);

        passwords[i] = pass;
        i++;
    }

CLEANUP:
    free(query);
    mysql_free_result(res);

    return row_count;
}

void
db_password_free(password_t *pass)
{
    if (pass == NULL) {
        return;
    }

    if (pass->name != NULL) {
        free(pass->name);
    }

    if (pass->username != NULL) {
        free(pass->username);
    }

    if (pass->password != NULL) {
        free(pass->password);
    }
}

// db_passwords_free frees the memory allocated for the 
// get passwords call.
void
db_passwords_free(password_t **passwords, const uint64_t size)
{
    if (passwords == NULL) {
        return;
    }

    for (uint64_t i = 0; i < size; i++) {
        if (passwords[i]->name != NULL) {
            free(passwords[i]->name);
        }

        if (passwords[i]->username != NULL) {
            free(passwords[i]->username);
        }

        if (passwords[i]->password != NULL) {
            free(passwords[i]->password);
        }

        free(passwords[i]);
    }
}

u_key_t*
db_key_new()
{
    u_key_t *key = malloc(sizeof(u_key_t));
    key->id = 0;
    key->key = calloc(0, sizeof(char));

    return key;
}

void
db_key_free(u_key_t *key)
{
    if (key == NULL) {
        return;
    }

    if (key->key != NULL) {
        free(key->key);
    }

    free(key);
}

int
db_key_add(db_t *db, const unsigned char key[32], const long user_id)
{
    MYSQL_STMT *insert_user_key_stmt = mysql_stmt_init(db->conn);

    int result = mysql_stmt_prepare(insert_user_key_stmt, INSERT_USER_KEY_QUERY, strlen(INSERT_USER_KEY_QUERY));  
    if (result != 0) {
        return result;
    }
    
    MYSQL_BIND bind[2];
    memset(bind, 0, sizeof(bind));

    unsigned int array_size = 1; 
    unsigned long key_len = strlen(key);

    bind[0].buffer_type = MYSQL_TYPE_STRING; 
    bind[0].buffer = (char *)key;
    bind[0].buffer_length = strlen(key); 
    bind[0].length = &key_len;

    bind[1].buffer_type = MYSQL_TYPE_LONG; 
    bind[1].buffer = (long*)&user_id; 
    bind[1].is_null = 0;
    bind[1].length = 0;
    
    if (mysql_stmt_bind_param(insert_user_key_stmt, bind)) {
        return result;
    }
    
    result = mysql_stmt_execute(insert_user_key_stmt); 
    if (result != 0) {
        return result;
    }

    return 0;
}

int
db_key_get_by_user_id(db_t *db, const long user_id, u_key_t *key)
{
    char sid[10 + sizeof(char)];
    sprintf(sid, "%ld", user_id);

    char *query = malloc(strlen(SELECT_KEY_BY_USER_ID_QUERY)+strlen(sid));
    sprintf(query, SELECT_KEY_BY_USER_ID_QUERY, user_id);

    if (mysql_query(db->conn, query)) {
        return -1;
    }

    MYSQL_RES *res = mysql_store_result(db->conn);
    uint64_t row_count = mysql_num_rows(res);
    if (row_count == 0) {
        goto CLEANUP;
    }

    while ((row = mysql_fetch_row(res)) != NULL) {
        key->key = realloc(key->key, strlen(row[0]));
        strcpy(key->key, row[0]);
    }

CLEANUP:
    free(query);
    mysql_free_result(res);

    return row_count;
}

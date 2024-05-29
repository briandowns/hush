#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "database.h"


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
    "password text NOT NULL," \
    "user_id int  NOT NULL," \
    "PRIMARY KEY (id)," \
    "FOREIGN KEY (user_id) REFERENCES users(id)," \
    "UNIQUE name_user_id_idx (name, user_id)" \
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

#define INSERT_PASSWORD_QUERY "INSERT INTO passwords (name, password, user_id) VALUES (?, ?, ?)"
#define INSERT_USER_QUERY "INSERT INTO users (username, first_name, last_name, password, token) VALUES (?, ?, ?, PASSWORD(?), ?)"
#define SELECT_ALL_USERS_QUERY "SELECT * FROM users"
#define SELECT_USER_BY_NAME_QUERY "SELECT * FROM users WHERE username = '%s'"
#define SELECT_USER_BY_ID_QUERY "SELECT * FROM users WHERE id = %lu"
#define SELECT_PASSWORD_BY_NAME_QUERY "SELECT * FROM passwords WHERE name = '%s' AND user_id = %lu"
#define SELECT_PASSWORD_BY_TOKEN_QUERY "SELECT * FROM passwords WHERE name = '%s' AND user_id = (SELECT id FROM users WHERE token = '%s')"
#define SELECT_TOKEN_BY_USERNAME_QUERY "SELECT token FROM users WHERE username = '%s'"

MYSQL_ROW row;

struct db {
    MYSQL *conn;
};

db_t* db_new() {
    return (struct db *)malloc(sizeof(struct db));
}

int db_init(db_t *db, char *server, char *user, char *password, char *database) {
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

    if (mysql_query(db->conn, CREATE_TABLE_LABELS_QUERY)) {
        return 4;
    }

    if (mysql_query(db->conn, CREATE_TABLE_PASSWORD_LABELS_QUERY)) {
        return 5;
    }

    db_add_user(db, "admin", "admin", "admin", "admin!", "");

    return 0;
}

int db_add_password(db_t *db, char *name, char *password, char *labels, long user_id) {
    MYSQL_STMT *insert_password_stmt = mysql_stmt_init(db->conn);

    int result = mysql_stmt_prepare(insert_password_stmt, INSERT_PASSWORD_QUERY, strlen(INSERT_PASSWORD_QUERY));  
    if (result != 0) {
        return result;
    }

    MYSQL_BIND bind[3];

    unsigned int array_size = 1;
    unsigned long password_len = strlen(password); 
    unsigned long name_len = strlen(name);

    memset(bind, 0, sizeof(bind)); 

    bind[0].buffer_type = MYSQL_TYPE_STRING; 
    bind[0].buffer = name;
    bind[0].buffer_length = strlen(name); 
    bind[0].length = &name_len;

    bind[1].buffer_type = MYSQL_TYPE_STRING; 
    bind[1].buffer = password;
    bind[1].buffer_length = strlen(password); 
    bind[1].length = &password_len;

    bind[2].buffer_type = MYSQL_TYPE_LONG; 
    bind[2].buffer = &user_id; 
    bind[2].is_null = 0;
    bind[2].length = 0;

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

int db_add_label(db_t *db, char *label, long pass_id) {

    return 0;
}

int db_add_user(db_t *db, char *username, char *first_name, char *last_name, char *password, char *token) {
    MYSQL_STMT *insert_user_stmt = mysql_stmt_init(db->conn);

    int result = mysql_stmt_prepare(insert_user_stmt, INSERT_USER_QUERY, strlen(INSERT_USER_QUERY));  
    if (result != 0) {
        return result;
    }
    
    MYSQL_BIND bind[5];

    unsigned int array_size = 1; 
    unsigned long username_len = strlen(username);
    unsigned long first_name_len = strlen(first_name);
    unsigned long last_name_len = strlen(last_name);
    unsigned long password_len = strlen(password);
    unsigned long token_len = strlen(token);

    memset(bind, 0, sizeof(bind)); 

    bind[0].buffer_type = MYSQL_TYPE_STRING; 
    bind[0].buffer = username;
    bind[0].buffer_length = strlen(username); 
    bind[0].length = &username_len;

    bind[1].buffer_type = MYSQL_TYPE_STRING; 
    bind[1].buffer = first_name;
    bind[1].buffer_length = strlen(first_name); 
    bind[1].length = &first_name_len;

    bind[2].buffer_type = MYSQL_TYPE_STRING; 
    bind[2].buffer = last_name;
    bind[2].buffer_length = strlen(last_name); 
    bind[2].length = &last_name_len;

    bind[3].buffer_type = MYSQL_TYPE_STRING; 
    bind[3].buffer = password;
    bind[3].buffer_length = strlen(password); 
    bind[3].length = &password_len;

    bind[4].buffer_type = MYSQL_TYPE_STRING; 
    bind[4].buffer = token;
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

uint64_t db_get_all_users(db_t *db, user_t **users) {
    if (mysql_query(db->conn, SELECT_ALL_USERS_QUERY)) {
        return -1;
    }

    MYSQL_RES *res = mysql_store_result(db->conn);
    uint64_t row_count = mysql_num_rows(res);

    users = realloc(users, sizeof(user_t)*row_count);

    uint64_t i = 0;
    char *endptr;

    while ((row = mysql_fetch_row(res)) != NULL) {
        user_t *user = malloc(sizeof(user_t));

        user->id = strtol(row[0], &endptr, 10);
        user->username = malloc(strlen(row[1]));
        strcpy(user->username, row[1]);

        user->first_name = malloc(strlen(row[2]));
        strcpy(user->first_name, row[2]);

        user->last_name = malloc(strlen(row[3]));
        strcpy(user->last_name, row[3]);

        user->password = malloc(strlen(row[4]));
        strcpy(user->password, row[4]);

        users[i] = user;
        i++;
    }

    mysql_free_result(res);

    return row_count;
}

int db_get_user_by_username(db_t *db, char *username, user_t *user) {
    char *query = malloc(strlen(SELECT_USER_BY_NAME_QUERY)+strlen(username));
    sprintf(query, SELECT_USER_BY_NAME_QUERY, username);

    if (mysql_query(db->conn, query)) {
        return -1;
    }

    MYSQL_RES *res = mysql_store_result(db->conn);
    uint64_t row_count = mysql_num_rows(res);

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

    free(query);
    mysql_free_result(res);

    return 0;
}

int db_get_user_by_id(db_t *db, long id, user_t *user) {
    char sid[10 + sizeof(char)];
    sprintf(sid, "%ld", id);

    char *query = malloc(strlen(SELECT_USER_BY_ID_QUERY)+strlen(sid));
    sprintf(query, SELECT_USER_BY_ID_QUERY, id);
    printf("%s\n", query);
    if (mysql_query(db->conn, query)) {
        return -1;
    }

    MYSQL_RES *res = mysql_store_result(db->conn);
    uint64_t row_count = mysql_num_rows(res);

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

    free(query);
    mysql_free_result(res);

    return 0;
}

char *db_get_user_token(db_t *db, char *username) {
    char *query = malloc(strlen(SELECT_TOKEN_BY_USERNAME_QUERY)+strlen(username));
    sprintf(query, SELECT_TOKEN_BY_USERNAME_QUERY, username);

    if (mysql_query(db->conn, query)) {
        return NULL;
    }
    
    MYSQL_RES *res = mysql_store_result(db->conn);
    uint64_t row_count = mysql_num_rows(res);

    char *token;

    while ((row = mysql_fetch_row(res)) != NULL) {
        token = malloc(strlen(row[0]));
        strcpy(token, row[0]);
    }

    free(query);
    mysql_free_result(res);

    return token;
}

// db_free_user_result frees the memory allocated for the 
// get user call.
void db_free_user_result(user_t *user) {
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

    free(user);
}

// db_free_users_result frees the memory allocated for the 
// get users call.
void db_free_users_result(user_t **user, uint64_t size) {
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

int db_get_password_by_name(db_t *db, char *name, long user_id, password_t *pass) {
    char *query = malloc(strlen(SELECT_PASSWORD_BY_NAME_QUERY)+strlen(name));
    sprintf(query, SELECT_PASSWORD_BY_NAME_QUERY, name, user_id);

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
        pass->password = malloc(strlen(row[2]));
        strcpy(pass->password, row[2]);
        pass->user_id = strtol(row[3], &endptr, 10);
    }

    free(query);
    mysql_free_result(res);

    return 0;
}

int db_get_password_by_token(db_t *db, char *name, char *token, password_t *pass) {
    char *query = malloc(strlen(SELECT_PASSWORD_BY_TOKEN_QUERY)+strlen(name)+strlen(token));
    sprintf(query, SELECT_PASSWORD_BY_TOKEN_QUERY, name, token);
    printf("XXX - query: %s\n", query);
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

        pass->password = malloc(strlen(row[2]));
        strcpy(pass->password, row[2]);
    }

    free(query);
    mysql_free_result(res);

    return 0;
}

void db_cleanup(db_t *db) {
    if (db == NULL) {
        return;
    }

    if (db->conn != NULL) {
        mysql_close(db->conn);
    }
    
    free(db);
}

const char *db_get_error(db_t *db) {
    return mysql_error(db->conn);
}

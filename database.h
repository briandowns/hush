/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Brian J. Downs
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _DATABASE_H
#define _DATABASE_H

#include <stdbool.h>

#include <mysql/mysql.h>


typedef struct db db_t;

typedef struct {
    long id;
    char *username;
    char *first_name;
    char *last_name;
    char *password;
    char *token;
} user_t;

typedef struct {
    long id;
    char *name;
    char *username;
    char *password;
    long user_id;
} password_t;

typedef struct {
    long id;
    char *key;
} u_key_t;

db_t*
db_new();

int
db_init(db_t *db, const char *server, const char *user, const char *password, const char *database);

const char*
db_get_error(db_t *db);

void
db_cleanup(db_t *db);

user_t*
db_user_new();

user_t**
db_users_new();

int
db_user_add(db_t *db, const char *username, const char *first_name, const char *last_name, const char *password, const char *token);

int
db_user_get_by_username(db_t *db, const char *username, user_t *user);

int
db_user_get_by_id(db_t *db, const long id, user_t *user);

int
db_user_get_by_token(db_t *db, const char *token, user_t *user);

int
db_user_get_token(db_t *db, const char *username, const char *password, user_t *user);

void
db_user_free(user_t *user);

uint64_t
db_users_get_all(db_t *db, user_t **users);

int
db_user_login(db_t *db, const char *username, const char *password);

void
db_users_free(user_t **user, const uint64_t size);

password_t*
db_password_new();

int
db_password_add(db_t *db, const char *name, const char *username, const char *password, const char *labels, const long user_id);

int
db_password_get_by_name(db_t *db, const char *name, const long user_id, password_t *pass);

int
db_password_get_by_token(db_t *db, const char *name, const char *token, password_t *pass);

u_key_t*
db_key_new();

void
db_key_free(u_key_t *key);

int
db_key_add(db_t *db, const unsigned char key[32], const long user_id);

int
db_key_get_by_user_id(db_t *db, const long user_id, u_key_t *key);

/**
 * db_pass_free frees the memory used by the given argument
*/
void
db_password_free(password_t *pass);

#endif /* _DATABASE_H */

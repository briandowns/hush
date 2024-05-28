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
    char *password;
    long user_id;
} password_t;

db_t* db_new();
int db_init(db_t *db, char *server, char *user, char *password, char *database);
void db_cleanup(db_t *db);

int db_add_user(db_t *db, char *username, char *first_name, char *last_name, char *password, char *token);
uint64_t db_get_all_users(db_t *db, user_t **users);
int db_get_user_by_username(db_t *db, char *username, user_t *user);
char *db_get_user_token(db_t *db, char *username);

int db_add_password(db_t *db, char *name, char *password, char *labels, long user_id);
int db_get_password_by_name(db_t *db, char *name, long user_id, password_t *pass);
const char *db_get_error(db_t *db);

#endif /* _DATABASE_H */

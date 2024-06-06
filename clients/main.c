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

#include <dirent.h>
#include <errno.h>
#include <libgen.h>
#ifdef __linux__
#include <linux/limits.h>
#else
#include <sys/syslimits.h>
#endif
#ifdef __linux__
#include <signal.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <curl/curl.h>
#include <jansson.h>
#include <sodium.h>

#include "../base64.h"
#include "../pass.h"

#define STR1(x) #x
#define STR(x) STR1(x)

#define USAGE                                                  \
    "usage: %s [-vh]\n"                                        \
    "  -v            version\n"                                \
    "  -h            help\n\n"                                 \
    "commands:\n"                                              \
    "  init          initialize hush\n"                        \
    "  get           retrieve a previously saved hushword\n"   \
    "  set           save a hushword\n"                        \
    "  ls,list       list hushwords\n"                         \
    "  rm,remove     delete a previously saved hushword\n"     \
    "  check         checks the given hushword's complexity\n" \
    "  gen,generate  generates a secure hushword\n"            \
    "  config        show current configuration\n"             \
    "  help          display the help menu\n"                  \
    "  version       show the version\n"

#define CONFIG_DIR ".hush"
#ifndef __linux__
#define DT_DIR 4
#endif

#define MESSAGE ((const unsigned char *) "oh yeah, it worked!")
#define MESSAGE_LEN 19
#define CIPHERTEXT_LEN (crypto_secretbox_MACBYTES + MESSAGE_LEN)

#define KEY_NAME "hush.key"

/**
 * COMMAND_ARG_ERR_CHECK checks to make sure the 
 * there is an argument to the given command if 
 * if not, print an error and exit.
 */
#define COMMAND_ARG_ERR_CHECK if (argc != 3) {                \
    fprintf(stderr, "error: %s requires argument\n", argv[i]); \
    return 1;                                                 \
}


static void
mkdir_p(const char *dir)
{
    char tmp[256];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp),"%s",dir);
    len = strlen(tmp);
    if (tmp[len - 1] == '/') {
        tmp[len - 1] = 0;
    }

    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            mkdir(tmp, S_IRWXU);
            *p = '/';
        }
    }

    mkdir(tmp, S_IRWXU);
}

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

    if (argc < 2) {
        printf(USAGE, STR(app_name));
        return 1;
    }

    srand(time(NULL));

    if (sodium_init() != 0) {
        return 1;
    }

    unsigned char key[crypto_secretbox_KEYBYTES];
    unsigned char nonce[crypto_secretbox_NONCEBYTES];
    unsigned char ciphertext[CIPHERTEXT_LEN];

    crypto_secretbox_keygen(key);
    randombytes_buf(nonce, sizeof(nonce));
    crypto_secretbox_easy(ciphertext, MESSAGE, MESSAGE_LEN, nonce, key);

    unsigned char decrypted[MESSAGE_LEN];
    if (crypto_secretbox_open_easy(decrypted, ciphertext, CIPHERTEXT_LEN, nonce, key) != 0) {
        /* message forged! */
    }

    CURLcode res;
 
    CURL *curl = curl_easy_init();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "version")) {
            printf("git: %s\n", STR(git_sha));
            return 0;
        }

        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "help")) {
            printf(USAGE, STR(bin_name));
            return 0;
        }

        char hush_dir_path[PATH_MAX];
        const char *hush_dir = getenv("HUSH_DIR");
        if ((hush_dir != NULL) && (hush_dir[0] != 0)) {
            strcpy(hush_dir_path, hush_dir);
        } else {
            strcpy(hush_dir_path, getenv("HOME"));
            strcat(hush_dir_path, "/" CONFIG_DIR);
        }

        char key_file_path[PATH_MAX];
        const char *hush_key = getenv("HUSH_KEY");
        if ((hush_key != NULL) && (hush_key[0] == 0)) {
            strcpy(key_file_path, hush_key);
        } else {
            strcpy(key_file_path, hush_dir_path);
            strcat(key_file_path, "/");
            strcat(key_file_path, KEY_NAME);
        }

        if (strcmp(argv[i], "init") == 0) {
            DIR* dir = opendir(hush_dir_path);
            if (ENOENT == errno) {
                mkdir(hush_dir_path, 0700);
            } else {
                closedir(dir);
            }
            
            if (access(key_file_path, F_OK) != 0) {
                curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8080/login");
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                // "{\"username\": \"bdowns\", \"password\": \"one4all\"}"

            }

            return 0;
        }

        if ((strcmp(argv[i], "ls") == 0) || (strcmp(argv[i], "list") == 0)) {
            if (argc == 3) {
                //
            }

            break;
        }

        if (strcmp(argv[i], "set") == 0) {
            COMMAND_ARG_ERR_CHECK;

            break;
        }

        if (strcmp(argv[i], "get") == 0) {
            COMMAND_ARG_ERR_CHECK;

            break;
        }

        if ((strcmp(argv[i], "rm") == 0) || (strcmp(argv[i], "remove") == 0)) {
            COMMAND_ARG_ERR_CHECK;

            break;
        }

        if ((strcmp(argv[i], "gen") == 0) || (strcmp(argv[i], "generate") == 0)) {
            COMMAND_ARG_ERR_CHECK;

            int size = atoi(argv[2]);
            char *pass = generate_password(size);

            printf("%s\n", pass);
            free(pass);

            break;
        }

        if (strcmp(argv[i], "check") == 0) {
            COMMAND_ARG_ERR_CHECK

            check(argv[2]);

            break;
        }
    }

    curl_easy_cleanup(curl);

    return 0;
}

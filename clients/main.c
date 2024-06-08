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
#include "../http.h"
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

#define BUFFER_SIZE (256 * 1024)


int count = 0;

struct result {
    unsigned int len; 
    char *data;
    unsigned int pos;
};

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

static size_t
handle_res(void *ptr, size_t size, size_t nmemb, void *data)
{
    struct result *result = (struct result *)data;

    if(result->pos + size * nmemb >= result->len - 1) { //make sure the buffer is large enough. if not, make it bigger.
        printf("error, buffer too small. %u * %u = %lu", (unsigned int)size, (unsigned int)nmemb, (unsigned int)size*nmemb);
        free(result->data);
        result->data = (char*)malloc((size*nmemb+1)*sizeof(char));
    }

    //this is done because curl can return data in chunks instead of 
    //full strings. so we have to keep track of where we have to 
    //add to the data string. 
    memcpy(result->data + result->pos, ptr, size * nmemb);
    result->pos += size * nmemb;

    return size * nmemb;
}

size_t
pass_buffer(char *bufptr, size_t size, size_t nitems, void *userp)
{
    char *d = (char *)userp;
    size_t s = (strlen(d) >= size*nitems) ? size*nitems-1 : strlen(d);

    memcpy(bufptr, d, s);
    bufptr[s+1] = '\0';
    count += 1;

    return s;
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

    CURL *curl = curl_easy_init();

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "version") == 0) {
            printf("git: %s\n", STR(git_sha));
            return 0;
        }

        if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "help") == 0) {
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
                json_t *doc = json_object();
                // "{\"username\": \"bdowns\", \"password\": \"one4all\"}"
                doc = json_pack("{s:s, s:s}", "username", "bdowns", "password", "one4all");

                char *buf = json_dumps(doc, JSON_INDENT(0));
                unsigned int len = strlen(buf);
                printf("\n%s\n\n",buf);
                json_error_t jsonerror;
                json_t *root = json_loads(buf, 0, &jsonerror);

                if (!json_is_object(root)){
                    printf("error: %u:%u: %s\n", jsonerror.line, jsonerror.column, jsonerror.text);
                    return -1;  
                }
                json_decref(root);

                struct result res;  //use this to save the return from the curl request
                res.data = (char*)malloc(BUFFER_SIZE*sizeof(char));;  //set the pointer to allocated space.
                res.len = BUFFER_SIZE;
                res.pos = 0;

                char error_buf[CURL_ERROR_SIZE+1];

                struct curl_slist *headers = NULL;
                headers = curl_slist_append(headers, "Content-Type: application/json");
                headers = curl_slist_append(headers, "Accept: application/json");

                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                curl_easy_setopt(curl, CURLOPT_URL, "http://localhost/login");
                curl_easy_setopt(curl, CURLOPT_PORT, 8080L);
                curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, HTTP_METHOD_POST);
                curl_easy_setopt(curl, CURLOPT_POST, 1L);
                curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, handle_res);
                curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);
                curl_easy_setopt(curl, CURLOPT_READFUNCTION, pass_buffer);
                curl_easy_setopt(curl, CURLOPT_READDATA, buf);
                curl_easy_setopt(curl, CURLOPT_INFILESIZE_LARGE, (curl_off_t)len);
                curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buf);
                curl_easy_setopt(curl, CURLOPT_UPLOAD, 1);
                //curl_easy_setopt(curl, CURLOPT_USERAGENT, "hush/0.1");

                curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

                CURLcode res_code = curl_easy_perform(curl);
                if(res_code != 0) { 
                    printf("Curl returned an error.\n");
                    printf("Error code: %u \n", res_code);
                    printf("Error: %s\n\n",error_buf);

                    return 1;
                }

                root = json_loads(res.data, 0, &jsonerror);
                
                const json_t *jt = json_object_get(root, "token");
                const char *token = json_string_value(jt);

                json_decref(root);
            }

            curl_easy_cleanup(curl);

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

    curl_global_cleanup();

    return 0;
}

#include <jansson.h>

#include "config.h"

config_t*
config_load(const char *file)
{
    FILE *fd = open(file);
    
    config_t *config = malloc(sizeof(config_t));
    config->db = malloc(sizeof(database_t));
    config->db->host  = calloc(0, sizeof(char));
    config->db->name  = calloc(0, sizeof(char));
    config->db->user  = calloc(0, sizeof(char));
    config->db->pass  = calloc(0, sizeof(char));

    config->http = malloc(sizeof(http_t));
    config->http->port = 8080;

    config->admin = malloc(sizeof(admin_t));
    config->admin->username = calloc(0, sizeof(char));
    config->admin->first_name = calloc(0, sizeof(char));
    config->admin->last_name = calloc(0, sizeof(char));
    config->admin->password = calloc(0, sizeof(char));
    
    return config;
}

void
config_free(config_t *config)
{
    if (config == NULL) {
        return;
    }

    if (config->db != NULL) {
        if (config->db->host != NULL) {
            free(config->db->host);
        }
        if (config->db->name != NULL) {
            free(config->db->name);
        }
        if (config->db->user != NULL) {
            free(config->db->user);
        }
        if (config->db->pass != NULL) {
            free(config->db->pass);
        }
    }

    if (config->http != NULL) {
        free(config->http);
    }

    if (config->admin != NULL) {
        if (config->admin->username != NULL) {
            free(config->admin->username);
        }

        if (config->admin->first_name != NULL) {
            free(config->admin->first_name);
        }

        if (config->admin->last_name != NULL) {
            free(config->admin->last_name);
        }

        if (config->admin->password != NULL) {
            free(config->admin->password);
        }
    }
}

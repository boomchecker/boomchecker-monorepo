#include <stdio.h>
#include <string.h>
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include "slre.h"

#define BASE_PATH       "/storage"
#define MAX_PATH_LENGTH 512

static const char* TAG = "GET_STATIC";

// --- MIME Type Helper ---
static const char* get_mime_type(const char* filepath) {
    if (strstr(filepath, ".html")) {
        return "text/html";
    }
    if (strstr(filepath, ".css")) {
        return "text/css";
    }
    if (strstr(filepath, ".js")) {
        return "application/javascript";
    }
    if (strstr(filepath, ".json")) {
        return "application/json";
    }
    if (strstr(filepath, ".svg")) {
        return "image/svg+xml";
    }
    if (strstr(filepath, ".ico")) {
        return "image/x-icon";
    }
    if (strstr(filepath, ".png")) {
        return "image/png";
    }
    if (strstr(filepath, ".jpg") || strstr(filepath, ".jpeg")) {
        return "image/jpeg";
    }
    if (strstr(filepath, ".woff2")) {
        return "font/woff2";
    }
    if (strstr(filepath, ".ttf")) {
        return "font/ttf";
    }
    return "application/octet-stream";
}

// --- Static file handler ---
static esp_err_t get_static_file_handler(httpd_req_t* req) {
    char filepath
        [MAX_PATH_LENGTH + 20 + 1
         + 5]; // 512 for path + 20 /storage/felix-local + 1 for null terminator + 5 for .gzip
    const char* uri = req->uri;

    // Redirect root URI and everything without extension to index.html
    // TODO: Make this more generic and configurable
    if (strcmp(uri, "/") == 0 || strrchr(uri, '.') == NULL) {
        uri = "/index.html";
    }

    snprintf(filepath, sizeof(filepath), "%s%s.gz", BASE_PATH, uri);
    ESP_LOGI(TAG, "Serving file: %s", filepath);

    FILE* file = fopen(filepath, "r");
    if (!file) {
        ESP_LOGE(TAG, "Gzip file not found: %s, fallback to normal.", filepath);
        snprintf(filepath, sizeof(filepath), "%s%s", BASE_PATH, uri);

        file = fopen(filepath, "r");
        if (!file) {
            ESP_LOGE(TAG, "File not found: %s", filepath);
            httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "File not found");
            return ESP_FAIL;
        } else {
            ESP_LOGI(TAG, "Normal file served: %s", filepath);
        }
    } else {
        httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    }

    // Set content type
    const char* mime = get_mime_type(filepath);
    httpd_resp_set_type(req, mime);

    char chunk[512];
    size_t read_bytes;
    while ((read_bytes = fread(chunk, 1, sizeof(chunk), file)) > 0) {
        httpd_resp_send_chunk(req, chunk, read_bytes);
    }

    fclose(file);
    httpd_resp_send_chunk(req, NULL, 0); // end response
    return ESP_OK;
}

// --- Route table ---
typedef struct {
    const char* pattern;
    esp_err_t (*handler)(httpd_req_t* req);
} static_route_entry_t;

static const static_route_entry_t static_route_table[] = {
    {"^/.*$", get_static_file_handler} // catch-all
};

// --- Regex-based router ---
static esp_err_t route_static_get_request(httpd_req_t* req) {
    struct slre_cap caps[1];

    for (size_t i = 0; i < sizeof(static_route_table) / sizeof(static_route_table[0]); i++) {
        if (slre_match(static_route_table[i].pattern, req->uri, strlen(req->uri), caps, 1, 0) > 0) {
            ESP_LOGI(TAG, "Routing static: %s", req->uri);
            return static_route_table[i].handler(req);
        }
    }

    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Static path not found");
    return ESP_FAIL;
}

// --- Public entry point for static requests ---
esp_err_t get_static_handler(httpd_req_t* req) {
    ESP_LOGI(TAG, "GET static request: %s", req->uri);
    return route_static_get_request(req);
}

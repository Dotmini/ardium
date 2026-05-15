/* runtime_common.cpp - Cross-platform Ardium Runtime Utilities */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* --- Generic String Utilities --- */
/* --- Generic String Utilities --- */
// MOVED TO ARDIUM (stdlib/CoreString.ar)

/* --- Math / Matrix (Stub/Generic) --- */
/* --- Math / Matrix (Stub/Generic) --- */
// MOVED TO ARDIUM (stdlib/CoreMath.ar)

/* --- Self Healing Stubs --- */
void setup_self_healing() {}
int recovery_mode() { return 0; }

/* --- File I/O --- */
void ardium_write_file(const char* path, const char* content) {
    if (!path || !content) return;
    printf("💾 Writing File: %s\n", path);
    FILE* f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s", content);
        fclose(f);
    }
}

const char* ardium_read_file(const char* path) {
    if (!path) return "";
    printf("📄 Reading File: %s\n", path);
    FILE* f = fopen(path, "r");
    if (!f) return "";
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(size + 1);
    fread(buf, 1, size, f);
    buf[size] = 0;
    fclose(f);
    return buf;
}

/* --- Crypto Stub --- */
const char* ardium_sha256(const char* input) {
    if (!input) return "";
    char* hash = (char*)malloc(65);
    snprintf(hash, 65, "sha256_stub_%lx", (unsigned long)strlen(input));
    return hash;
}


/* --- Network (BSD Sockets for Linux/Unix) --- */
#ifndef __APPLE__
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

const char* ardium_http_get(const char* url) {
    if (!url) return "";
    printf("🌐 HTTP GET (Native Socket): %s\n", url);
    
    // Very basic HTTP/1.0 GET implementation
    // 1. Parse Host/Path
    // Warning: No HTTPS support in this raw socket example for simplicity.
    // Real-world would need OpenSSL or GnuTLS for stable HTTPS.
    
    char host[256];
    char path[1024];
    int port = 80;
    
    // Naive parsing: http://host/path
    const char* p = strstr(url, "://");
    const char* domain_start = p ? p + 3 : url;
    const char* path_start = strchr(domain_start, '/');
    
    if (path_start) {
        size_t host_len = path_start - domain_start;
        if(host_len > 255) host_len = 255;
        strncpy(host, domain_start, host_len);
        host[host_len] = '\0';
        strncpy(path, path_start, 1023);
    } else {
        strncpy(host, domain_start, 255);
        strcpy(path, "/");
    }
    
    // 2. Resolve Host
    struct hostent *server = gethostbyname(host);
    if (server == NULL) {
        printf("❌ DNS Resolution Failed for %s\n", host);
        return "";
    }
    
    // 3. Create Socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return "";
    
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
    serv_addr.sin_port = htons(port);
    
    // 4. Connect
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("❌ Connection Failed\n");
        close(sockfd);
        return "";
    }
    
    // 5. Send Request
    char request[2048];
    snprintf(request, sizeof(request), 
        "GET %s HTTP/1.0\r\nHost: %s\r\nUser-Agent: ArdiumRuntime/2.0\r\n\r\n", 
        path, host);
    
    if (send(sockfd, request, strlen(request), 0) < 0) {
        close(sockfd);
        return "";
    }
    
    // 6. Read Response
    // We'll just read into a fixed buffer for this demo
    // Real implementation needs dynamic buffer growing
    size_t capacity = 1024 * 64; // 64KB
    char* response = (char*)malloc(capacity);
    size_t total_read = 0;
    ssize_t n;
    
    while ((n = recv(sockfd, response + total_read, capacity - total_read - 1, 0)) > 0) {
        total_read += n;
        if (total_read >= capacity - 1) break; 
    }
    response[total_read] = '\0';
    close(sockfd);
    
    // 7. Strip Headers (Naive)
    char* body = strstr(response, "\r\n\r\n");
    if (body) {
        return strdup(body + 4);
    }
    return response;
}
#endif

#ifdef __cplusplus
} // extern "C"
#endif

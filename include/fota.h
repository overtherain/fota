#include <stdio.h>//printf
#include <sys/io.h>
#include <string.h>//字符串处理
#include <sys/socket.h>//套接字
#include <arpa/inet.h>//ip地址处理
#include <fcntl.h>//open系统调用
#include <unistd.h>//write系统调用
#include <netdb.h>//查询DNS
#include <stdlib.h>//exit函数
#include <sys/stat.h>//stat系统调用获取文件大小
#include <sys/time.h>//获取下载时间
#include <time.h>

#define DEBUG

unsigned long tmp_size=0;
unsigned long full_size = 0;

int file_exist = 0;
int send_again = 0;

int SUCCESS_OK = 1;
int ERROR_FAIL = -1;
int ERROR_INPUT_IP = -3;
int ERROR_PARSE_URL = -4;
int ERROR_CREATE_SOCKET_FAILED = -5;
int ERROR_SEND_HEADER_FAILED = -6;
int ERROR_HTTP_GET_FAILED = -7;
int ERROR_DOWNLOAD_FAILED = -8;
int ERROR_HTTP_STATUS_CODE = -9;

typedef struct HTTP_RES_HEADER//保持相应头信息
{
    int status_code;//HTTP/1.1 '200' OK
    char content_type[128];//Content-Type: application/gzip
    char content_range[128];//Content-Range: bytes 500-999/1000
    char accept_ranges[128];//Accept-Ranges: bytes
    unsigned long content_length;//Content-Length: 11683079
    char file_name[256];
    char ip_addr[16];
}HTTP_RES_HEADER;

typedef struct HOST_INFO
{
    char url[2048];
    char host[64];
    int port;
    char ip_addr[16];
    char new_file_name[256];
}HOST_INFO;

HOST_INFO host_info;
HTTP_RES_HEADER tmp_http_res_header;
HTTP_RES_HEADER full_http_res_header;
struct sockaddr_in addr;

void init();
void free_all();

void parse_url(const char *url, char *host, int *port, char *file_name);
void get_ip_addr(char *host_name, char *ip_addr);
void progress_bar(long cur_size, long total_size, double speed);
void download(int client_socket);
void continue_download(int client_socket);
void parse_header(const char *response);
void get_http_response(int client_socket);

int send_http_header(int client_socket);
int http_get_full();
int http_get_tmp();
int get_host_info();
int check_download_file();
int parse_http_header(int client_socket, HTTP_RES_HEADER *http_res_header);

unsigned long get_file_size(const char *filename);

#ifdef DEBUG
    #define log_d(format, ...)  \
    {                           \
        time_t t = time(0);     \
        struct tm ttt = *localtime(&t);     \
        fprintf(stdout,     \
        "[DEBUG] [%5d_%02d_%02d %02d:%02d:%02d] [%s:%d] " \
        format "",              \
        ttt.tm_year + 1900,     \
        ttt.tm_mon + 1,         \
        ttt.tm_mday,            \
        ttt.tm_hour,            \
        ttt.tm_min,             \
        ttt.tm_sec,             \
        __FUNCTION__,__LINE__, ##__VA_ARGS__);  \
    }
#else
    #define log_d(format, ...){}
#endif /* DEBUG */

#define log_e(format, ...)  \
{                           \
    time_t t = time(0);     \
    struct tm ttt = *localtime(&t);     \
    fprintf(stdout,     \
    "[ERROR] [%5d_%02d_%02d %02d:%02d:%02d] [%s:%d] " \
    format "",              \
    ttt.tm_year + 1900,     \
    ttt.tm_mon + 1,         \
    ttt.tm_mday,            \
    ttt.tm_hour,            \
    ttt.tm_min,             \
    ttt.tm_sec,             \
    __FUNCTION__,__LINE__, ##__VA_ARGS__);  \
}

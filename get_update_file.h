#ifndef _GET_UPDATE_FILE_H_
#define _GET_UPDATE_FILE_H_

#include <stdio.h>
//#include <sys/io.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <netdb.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include "cutils/properties.h"
#include <utils/Log.h>

#define SUCCESS_ALREADY_DOWNLOAD              3
#define SUCCESS_HAVE_UPDATE                   2
#define SUCCESS_OK                            1
#define ERROR_FAIL                           -1
#define ERROR_INPUT_IP                       -3
#define ERROR_PARSE_URL                      -4
#define ERROR_CREATE_SOCKET_FAILED           -5
#define ERROR_SEND_HEADER_FAILED             -6
#define ERROR_HTTP_GET_FAILED                -7
#define ERROR_DOWNLOAD_FAILED                -8
#define ERROR_HTTP_STATUS_CODE               -9
#define ERROR_FILE_OPEN_FAILED              -10
#define ERROR_INVALID_URL                   -11
#define ERROR_INVALID_FILE_NAME             -12
#define ERROR_RENAME_FILE                   -13
#define ERROR_DOWNLOAD_LESSED               -14
#define ERROR_WRITE_FILE                    -15
#define ERROR_CONNECT_SOCKET_FAILED         -16
#define ERROR_INVALID_URL_REPONSE_ITEMS     -17
#define ERROR_INVALID_REPONSE_STATUS_CODE   -18

//#define DEBUG 1
#define LOG_TAG "FOTA"
#define RUN_ANDROID_LOG

#ifdef RUN_ANDROID_LOG
    #define log_d(format, args...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, format, ##args)
    #define log_e(format, args...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, format, ##args)
#else
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
#endif

typedef struct HTTP_RESPONSE_HEADER//保持相应头信息
{
    int status_code;                //HTTP/1.1 '200' OK
    char content_type[128];         //Content-Type: application/gzip
    char content_range[128];        //Content-Range: bytes 500-999/1000
    char accept_ranges[128];        //Accept-Ranges: bytes
    unsigned long content_length;   //Content-Length: 11683079
    char transfer_encoding[16];     //Transfer-Encoding: chunked
    char file_name[256];
    char ip_addr[16];
}HTTP_RESPONSE_HEADER;

typedef struct HOST_INFO
{
    char url[2048];
    char host[64];
    int port;
    char ip_addr[16];
    char md5[64];
    char new_file_name[256];
}HOST_INFO;

void parse_url(const char *url, char *host, int *port, char *file_name);
void get_ip_addr(char *host_name, char *ip_addr);
int send_http_header(int client_socket, int again, HOST_INFO *host_info);
int parse_http_response_header(int client_socket, HTTP_RESPONSE_HEADER *http_res_header);
int get_update_file(const char *url, const char *new_file_name);

#endif /* _GET_UPDATE_FILE_H_ */
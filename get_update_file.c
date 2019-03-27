#include "get_update_file.h"

unsigned long tmp_size=0;
unsigned long full_size = 0;

static int file_exist = 0;
static int send_again = 0;

static struct sockaddr_in addr;
static HOST_INFO host_info;
static HTTP_RESPONSE_HEADER tmp_http_res_header;
static HTTP_RESPONSE_HEADER full_http_res_header;

void get_ip_addr(char *host_name, char *ip_addr)
{
    //log_d("get_ip_addr\n");
    int i = 0;
    struct hostent *host = gethostbyname(host_name);
    if (!host){
        log_e("%s:%d get host from host_name failed\n", __FUNCTION__, __LINE__);
        ip_addr = NULL;
        return;
    }
    
    for (i = 0; host->h_addr_list[i]; i++){
        strcpy(ip_addr, inet_ntoa( * (struct in_addr*) host->h_addr_list[i]));
        log_d("%s:%d get host from host_name, ip_addr:%s\n", __FUNCTION__, __LINE__, ip_addr);
        break;
    }
}

void parse_url(const char *url, char *host, int *port, char *file_name)
{
    log_d("%s:%d parse_url\n", __FUNCTION__, __LINE__);
    /*通过url解析出域名, 端口, 以及文件名*/
    int i,j = 0;
    int start = 0;
    *port = 80;
    char *patterns[] = {"http://", "https://", NULL};
    char tmp_file[1024];
    //分离下载地址中的http协议
    for (i = 0; patterns[i]; i++){
        if (strncmp(url, patterns[i], strlen(patterns[i])) == 0){
            start = strlen(patterns[i]);
        }
    }
    //解析域名, 这里处理时域名后面的端口号会保留
    i = 0;
    for (i = start; url[i] != '/' && url[i] != '\0'; i++, j++) {
        host[j] = url[i];
    }
    host[j] = '\0';
    //解析端口号, 如果没有, 那么设置端口为80
    char *pos = strstr(host, ":");
    if (pos){
        sscanf(pos, ":%d", port);
    }
    //删除域名端口号
    i = 0;
    for (i = 0; i < (int)strlen(host); i++){
        if (host[i] == ':'){
            host[i] = '\0';
            break;
        }
    }
    //获取下载文件名
    j = 0;
    i = 0;
    for (i = start; url[i] != '\0'; i++){
        if (url[i] == '/'){
            if (i !=  (strlen(url) - 1)){
                j = 0;
            }
            continue;
        }else{
            tmp_file[j++] = url[i];
        }
    }
    tmp_file[j] = '\0';
    sprintf(file_name, "/data/%s", tmp_file);
    log_d("%s:%d parse_url tmp_file %s, file_name : %s\n", __FUNCTION__, __LINE__, tmp_file, file_name);
}

int get_host_info(void)
{
    //log_d("get_host_info\n");
    parse_url(host_info.url, host_info.host, &host_info.port, full_http_res_header.file_name);
    get_ip_addr(host_info.host, host_info.ip_addr);         //从url中分析出主机名, 端口号, 文件名
    strcpy(full_http_res_header.ip_addr, host_info.ip_addr);
    strcpy(tmp_http_res_header.file_name, full_http_res_header.file_name);
    strcpy(tmp_http_res_header.ip_addr, full_http_res_header.ip_addr);
    
    if (strlen(host_info.ip_addr) == 0){
        log_e("%s:%d parse_url failed\n", __FUNCTION__, __LINE__);
        return ERROR_PARSE_URL;
    }else{
        log_d("%s:%d parse_url success\n", __FUNCTION__, __LINE__);
        log_d("%s:%d URL: %s\n", __FUNCTION__, __LINE__, host_info.url);
        log_d("%s:%d Host: %s\n", __FUNCTION__, __LINE__, host_info.host);
        log_d("%s:%d IP Address: %s\n", __FUNCTION__, __LINE__, host_info.ip_addr);
        log_d("%s:%d Port: %d\n", __FUNCTION__, __LINE__, host_info.port);
        log_d("%s:%d FileName : %s\n", __FUNCTION__, __LINE__, full_http_res_header.file_name);
        
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(host_info.ip_addr);
        addr.sin_port = htons(host_info.port);
        
        if(!access(full_http_res_header.file_name,0)){
            file_exist = 1;
            log_d("%s:%d file exist\n", __FUNCTION__, __LINE__);
        }else{
            file_exist = 0;
            log_d("%s:%d file not exist\n", __FUNCTION__, __LINE__);
        }
        
        return SUCCESS_OK;
    }
}

static void progress_bar(const char *file_name,float sum, float file_size)
{
    float percent = (sum / file_size) * 100;
    
    char *sign = "#";
    if ((int)percent != 0){
        sign = (char *)malloc((int)percent + 1);
        strncpy(sign,"==================================================",(int) percent);
    }
    printf("%s %7.2f%% [%-*.*s] %.2f/%.2f mb\r",file_name,percent,50,(int)percent / 2,sign,sum / 1024.0 / 1024.0,file_size / 1024.0 / 1024.0);
    if ((int)percent != 0)
        free(sign);
    fflush(stdout);
}

static unsigned long get_file_size(const char *filename)
{
    //log_d("get_file_size\n");
    //通过系统调用直接得到文件的大小
    struct stat buf;
    if (stat(filename, &buf) < 0){
        return 0;
    }
    return (unsigned long) buf.st_size;
}

static unsigned long file_index(char* path)
{
    FILE *fp;
    unsigned long last;
    if((fp = fopen(path,"ab+")) == NULL)
    {
        log_d("%s:%d can not open %s\n", __FUNCTION__, __LINE__, path);
        return 0;
    }else{
        fseek(fp,0L,SEEK_END);
        last=ftell(fp);
        fclose(fp);
        return last;
    }
}

static int check_download_file(void)
{
    //log_d("check_download_file\n");
    int ret = 0;
    full_size = get_file_size(full_http_res_header.file_name);
    if (full_http_res_header.content_length == full_size){
        log_d("%s:%d download %s success!\n\n", __FUNCTION__, __LINE__, full_http_res_header.file_name);
        if(strlen(host_info.new_file_name) != 0){
            ret = rename(full_http_res_header.file_name, host_info.new_file_name);
            if(ret != 0){
                log_d("%s:%d rename file %s to %s failed\n\n", __FUNCTION__, __LINE__, full_http_res_header.file_name, host_info.new_file_name);
                return ERROR_RENAME_FILE;
            }else if(ret == 0){
                log_d("%s:%d rename file to %s\n\n", __FUNCTION__, __LINE__, host_info.new_file_name);
            }
        }
        ret = SUCCESS_OK;
    }else if(full_http_res_header.content_length > full_size){
        ret = ERROR_DOWNLOAD_FAILED;
        log_e("%s:%d 文件下载中有字节缺失, 下载失败, 请重试! content_length:%ld, file size:%ld\n\n", __FUNCTION__, __LINE__, full_http_res_header.content_length, full_size);
    }else{
        ret = ERROR_DOWNLOAD_LESSED;
        log_e("%s:%d 文件%s下载未完成! 已下:%ld, total:%ld\n\n", __FUNCTION__, __LINE__, full_http_res_header.file_name, full_size, full_http_res_header.content_length);
    }
    return ret;
}

//http请求头信息
int send_http_header(int client_socket, int again, HOST_INFO *host_info)
{
    //log_d("send_http_header\n");
    char header[2048] = {0};
    
    if(again){
        tmp_size = file_index(tmp_http_res_header.file_name);
        if(tmp_size == full_http_res_header.content_length){
            log_e("%s:%d file has already download\n", __FUNCTION__, __LINE__);
            return SUCCESS_ALREADY_DOWNLOAD;
        }
        //tmp_size = get_file_size(tmp_http_res_header.file_name);
        if(tmp_size > 0){
            sprintf(header, \
                "GET %s HTTP/1.1\r\n"\
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"\
                "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537(KHTML, like Gecko) Chrome/47.0.2526Safari/537.36\r\n"\
                "Host: %s:%d\r\n"\
                "Range: bytes=%ld-\r\n"\
                "Connection: keep-alive\r\n\r\n", host_info->url, host_info->host, host_info->port, tmp_size);
        }
    }else{
        sprintf(header, \
            "GET %s HTTP/1.1\r\n"\
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"\
            "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537(KHTML, like Gecko) Chrome/47.0.2526Safari/537.36\r\n"\
            "Host: %s:%d\r\n"\
            "Connection: keep-alive\r\n\r\n", host_info->url, host_info->host, host_info->port);
    }
    log_d("%s:%d Tmp_Header\n\n%s", __FUNCTION__, __LINE__, header);
    return write(client_socket, header, strlen(header));
}

int parse_http_response_header(int client_socket, HTTP_RESPONSE_HEADER *http_res_header)
{
    //log_d("parse_http_header\n");
    char buffer[BUFSIZ],temp[BUFSIZ],*ptr;
    bzero(buffer,sizeof(buffer));
    bzero(temp,sizeof(temp));
    int len,n = 0;
    while((len = read(client_socket, buffer, 1)) != 0){
        //log_d("parse_http_response_header in while:%d\n",n);
        temp[n] = *buffer;
        if (*buffer == '\n'){
            //log_d("parse_http_header read temp:%s\n",temp);
            ptr = strstr(temp,"HTTP/1.1");
            if (ptr != NULL){
                ptr = strchr(ptr,' ');
                ptr++;
                http_res_header->status_code = atoi(ptr);
            }
            ptr = strstr(temp,"Content-Length:");
            if (ptr != NULL){
                ptr = strchr(ptr,':');
                ptr++;
                http_res_header->content_length = strtoul(ptr,NULL,10);
            }
            ptr = strstr(temp,"Content-Type:");
            if (ptr != NULL){
                ptr = strchr(ptr,':');
                ptr++;
                strcpy(http_res_header->content_type,ptr);
                http_res_header->content_type[strlen(ptr) - 1] = '\0';
            }
            ptr = strstr(temp,"Content-Range:");
            if (ptr != NULL){
                ptr = strchr(ptr,':');
                ptr++;
                strcpy(http_res_header->content_range,ptr);
                http_res_header->content_range[strlen(ptr) - 1] = '\0';
            }
            ptr = strstr(temp,"Transfer-Encoding:");
            if (ptr != NULL){
                ptr = strchr(ptr,':');
                ptr++;
                strcpy(http_res_header->transfer_encoding,ptr);
                http_res_header->transfer_encoding[strlen(ptr) - 1] = '\0';
            }
            if (temp[0] == '\r' && temp[1] == '\n')
                break;
            bzero(temp,sizeof(temp));
            n = -1;
        }
        n++;
    }
    /*log_d("parse_http_response_header out while:%d\n",n);
    log_d("parse_http_header success : \n\n");
    log_d("http_res_header Status-Code: %d\n", http_res_header->status_code);
    log_d("http_res_header Content-Type: %s\n", http_res_header->content_type);
    log_d("http_res_header Content-Range: %s\n", http_res_header->content_range);
    log_d("http_res_header Content-Length: %ld bytes\n", http_res_header->content_length);
    log_d("http_res_header FileName: %s \n", http_res_header->file_name);
    log_d("http_res_header IPAddress: %s\n\n", http_res_header->ip_addr);*/
    
    return client_socket;
}

int download(int client_socket)
{
    int ret = 1;
    int len;
    unsigned long sum = 0;
    char buffer[BUFSIZ] = { 0 };
    FILE * fp;
    
    log_d("%s:%d start download file:%s\n", __FUNCTION__, __LINE__, full_http_res_header.file_name);
    fp = fopen(tmp_http_res_header.file_name, "ab+");
    
    if (fp == NULL){
        log_e("%s:%d file open failed!\n", __FUNCTION__, __LINE__);
        return ERROR_FILE_OPEN_FAILED;
    }
    while((len = read(client_socket,buffer,sizeof(buffer))) > 0){
        ret = fwrite(&buffer, len, 1, fp);
        if(ret != 1){
            fflush(fp);
            fclose(fp);
            int ch = chmod(tmp_http_res_header.file_name, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
            if(ch != 0){
                log_e("%s:%d chmod file failed!\n", __FUNCTION__, __LINE__);
            }
            return ERROR_WRITE_FILE;
        }
        sum += len;
        //progress_bar(full_http_res_header.file_name,(float)sum,(float)full_http_res_header.content_length);
        if (full_http_res_header.content_length == sum){
            printf("\n");
            break;
        }
    }
    fflush(fp);
    fclose(fp);
    int ch = chmod(tmp_http_res_header.file_name, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
    if(ch != 0){
        log_e("%s:%d chmod file failed!\n", __FUNCTION__, __LINE__);
    }
    return ret;
}

int continue_download(int client_socket)
{
    int ret = 1;
    int len;
    unsigned long sum = 0;
    char buffer[BUFSIZ] = { 0 };
    FILE * fp;
    
    log_d("%s:%d start continue download file:%s\n", __FUNCTION__, __LINE__, tmp_http_res_header.file_name);
    fp = fopen(tmp_http_res_header.file_name, "ab+");
    
    if (fp == NULL){
        log_e("%s:%d file open failed!\n", __FUNCTION__, __LINE__);
        ret = ERROR_FILE_OPEN_FAILED;
        return ret;
    }else{
        while((len = read(client_socket,buffer,sizeof(buffer))) > 0){
            ret = fwrite(&buffer, len, 1, fp);
            if(ret != 1){
                fflush(fp);
                fclose(fp);
                return ERROR_WRITE_FILE;
            }
            sum += len;
            //progress_bar(tmp_http_res_header.file_name,(float)sum,(float)tmp_http_res_header.content_length);
            if (tmp_http_res_header.content_length == sum){
                printf("\n");
                break;
            }
        }
    }
    fflush(fp);
    fclose(fp);
    return ret;
}

int http_get_full(void)
{
    int ret = 0;
    //log_d("http_get_full\n");
    //log_d("create socket...\n");
    int client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket < 0){
        log_e("%s:%d create socket failed: %d\n", __FUNCTION__, __LINE__, client_socket);
        return ERROR_CREATE_SOCKET_FAILED;
    }
    
    //连接远程主机
    log_d("%s:%d connect to host...\n", __FUNCTION__, __LINE__);
    int res = connect(client_socket, (struct sockaddr *) &addr, sizeof(addr));
    if (res == -1){
        log_e("%s:%d screate connect failed, error : %d\n", __FUNCTION__, __LINE__, res);
        return ERROR_CONNECT_SOCKET_FAILED;
    }else{
        ret = send_http_header(client_socket, send_again, &host_info);
    }
    
    if(ret == -1){
        log_e("%s:%d send_http_header failed: %d\n", __FUNCTION__, __LINE__, ret);
        close(client_socket);
        return ERROR_SEND_HEADER_FAILED;
    }else{
        log_d("%s:%d send_http_header success: %d\n", __FUNCTION__, __LINE__, ret);
        ret = parse_http_response_header(client_socket, &full_http_res_header);
        //get_http_response(client_socket);
        
        if (full_http_res_header.status_code != 200){
            log_e("%s:%d download failed, statuc code: %d\n", __FUNCTION__, __LINE__, full_http_res_header.status_code);
            ret = ERROR_HTTP_STATUS_CODE;
        }else if(!file_exist){
            send_again = 0;
            log_d("%s:%d downloading...\n", __FUNCTION__, __LINE__);
            ret = download(client_socket);
            if(!ret){
                close(client_socket);
                return ret;
            }
            ret = check_download_file();
        }else{
            send_again = 1;
            ret = SUCCESS_OK;
        }
    }
    close(client_socket);
    return ret;
}

int http_get_tmp(void)
{
    int ret = 0;
    //log_d("http_get_tmp\n");
    //log_d("create socket...\n");
    int client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket < 0){
        log_e("%s:%d create socket failed: %d\n", __FUNCTION__, __LINE__, client_socket);
        return ERROR_CREATE_SOCKET_FAILED;
    }
    
    //连接远程主机
    log_d("%s:%d connect to host...\n", __FUNCTION__, __LINE__);
    int res = connect(client_socket, (struct sockaddr *) &addr, sizeof(addr));
    if (res == -1){
        log_e("%s:%d create connect failed, error: %d\n", __FUNCTION__, __LINE__, res);
        return ERROR_CONNECT_SOCKET_FAILED;
    }else{
        ret = send_http_header(client_socket, send_again, &host_info);
    }
    
    if(ret == -1){
        log_e("%s:%d send_http_header failed: %d\n", __FUNCTION__, __LINE__, ret);
        return ERROR_SEND_HEADER_FAILED;
    }else if(ret == 3){
        return SUCCESS_ALREADY_DOWNLOAD;
    }else{
        log_e("%s:%d send_http_header success: %d\n", __FUNCTION__, __LINE__, ret);
        ret = parse_http_response_header(client_socket, &tmp_http_res_header);
        //get_http_response(client_socket);
        
        if (tmp_http_res_header.status_code != 206){
            log_e("%s:%d download failed, statuc code: %d\n", __FUNCTION__, __LINE__, tmp_http_res_header.status_code);
            ret = ERROR_HTTP_STATUS_CODE;
        }else{
            log_d("%s:%d downloading...\n", __FUNCTION__, __LINE__);
            ret = continue_download(client_socket);
            if(!ret){
                return ret;
            }
            ret = check_download_file();
        }
        close(client_socket);
    }
    
    return ret;
}

static void init(void)
{
    //log_d("====init====\n");
    bzero(&host_info,sizeof(host_info));
    bzero(&full_http_res_header,sizeof(full_http_res_header));
    bzero(&tmp_http_res_header,sizeof(tmp_http_res_header));
    bzero(&addr,sizeof(addr));
    file_exist = 0;
    send_again = 0;
}

static void free_all(void)
{
    //log_d("====free_all====\n");
    bzero(&host_info,sizeof(host_info));
    bzero(&full_http_res_header,sizeof(full_http_res_header));
    bzero(&tmp_http_res_header,sizeof(tmp_http_res_header));
    bzero(&addr,sizeof(addr));
}

int get_update_file(const char *url, const char *new_file_name)
{
    int ret = 0;
    init();
    
    if(url != NULL && strlen(url) != 0){
        strcpy(host_info.url, url);
    }else{
        return ERROR_INVALID_URL;
    }
    if(strlen(new_file_name) != 0){
        strcpy(host_info.new_file_name, new_file_name);
    }else{
        log_e("%s:%d not input target name\n", __FUNCTION__, __LINE__);
    }
    ret = get_host_info();
    if(!ret){
        log_e("%s:%d get_host_info error!\n", __FUNCTION__, __LINE__);
        return ret;
    }
    
    ret = http_get_full();
    if(!ret){
        log_e("%s:%d http get error : %d\n", __FUNCTION__, __LINE__, ret);
        return ret;
    }
    if(send_again){
        ret = http_get_tmp();
        if(!ret){
            log_e("%s:%d http get error : %d\n", __FUNCTION__, __LINE__, ret);
            return ret;
        }
    }
    
    free_all();
    return ret;
}

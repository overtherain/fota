#include "include/fota.h"

void parse_url(const char *url, char *host, int *port, char *file_name)
{
    log_d("parse_url\n");
    /*通过url解析出域名, 端口, 以及文件名*/
    int i,j = 0;
    int start = 0;
    *port = 80;
    char *patterns[] = {"http://", "https://", NULL};
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
            if (i !=  strlen(url) - 1){
                j = 0;
            }
            continue;
        }else{
            file_name[j++] = url[i];
        }
    }
    file_name[j] = '\0';
}

void parse_header(const char *response)
{
    log_d("parse_header\n");
    /*获取响应头的信息*/
    //获取返回代码
    if(send_again){
        char *pos = strstr(response, "HTTP/");
        if (pos){
            sscanf(pos, "%*s %d", &tmp_http_res_header.status_code);
        }
        //获取返回文档类型
        pos = strstr(response, "Content-Type:");
        if (pos){
            sscanf(pos, "%*s %s", tmp_http_res_header.content_type);
        }
        //获取返回文档起始点
        pos = strstr(response, "Content-Range:");
        if (pos){
            sscanf(pos, "%*s %s", tmp_http_res_header.content_range);
        }
        //获取返回文档单位
        pos = strstr(response, "Accept-Ranges:");
        if (pos){
            sscanf(pos, "%*s %s", tmp_http_res_header.accept_ranges);
        }
        //获取返回文档长度
        pos = strstr(response, "Content-Length:");
        if (pos){
            sscanf(pos, "%*s %ld", &tmp_http_res_header.content_length);
        }
        log_d(">>>>http响应头解析成功:<<<<\n\n");
        log_d("tmp_http_res_header Status-Code: %d\n", tmp_http_res_header.status_code);
        log_d("tmp_http_res_header Content-Type: %s\n", tmp_http_res_header.content_type);
        log_d("tmp_http_res_header Accept-Ranges: %s\n", tmp_http_res_header.accept_ranges);
        log_d("tmp_http_res_header Content-Range: %s\n", tmp_http_res_header.content_range);
        log_d("tmp_http_res_header Content-Length: %ld bytes\n\n", tmp_http_res_header.content_length);
        
        log_d("tmp_http_res_header FileName: %s \n", tmp_http_res_header.file_name);
        log_d("tmp_http_res_header IPAddress: %s\n\n", tmp_http_res_header.ip_addr);
    }else{
        char *pos = strstr(response, "HTTP/");
        if (pos){
            sscanf(pos, "%*s %d", &full_http_res_header.status_code);
        }
        //获取返回文档类型
        pos = strstr(response, "Content-Type:");
        if (pos){
            sscanf(pos, "%*s %s", full_http_res_header.content_type);
        }
        //获取返回文档起始点
        pos = strstr(response, "Content-Range:");
        if (pos){
            sscanf(pos, "%*s %s", full_http_res_header.content_range);
        }
        //获取返回文档单位
        pos = strstr(response, "Accept-Ranges:");
        if (pos){
            sscanf(pos, "%*s %s", full_http_res_header.accept_ranges);
        }
        //获取返回文档长度
        pos = strstr(response, "Content-Length:");
        if (pos){
            sscanf(pos, "%*s %ld", &full_http_res_header.content_length);
        }
        log_d(">>>>http响应头解析成功:<<<<\n\n");
        log_d("full_http_res_header Status-Code: %d\n", full_http_res_header.status_code);
        log_d("full_http_res_header Content-Type: %s\n", full_http_res_header.content_type);
        log_d("full_http_res_header Accept-Ranges: %s\n", full_http_res_header.accept_ranges);
        log_d("full_http_res_header Content-Range: %s\n", full_http_res_header.content_range);
        log_d("full_http_res_header Content-Length: %ld bytes\n\n", full_http_res_header.content_length);
        
        log_d("full_http_res_header FileName: %s \n", full_http_res_header.file_name);
        log_d("full_http_res_header IPAddress: %s\n\n", full_http_res_header.ip_addr);
    }
}

void get_ip_addr(char *host_name, char *ip_addr)
{
    log_d("get_ip_addr\n");
    int i = 0;
    struct hostent *host = gethostbyname(host_name);
    if (!host){
        log_e("get host from host_name failed\n");
        ip_addr = NULL;
        return;
    }
    
    for (i = 0; host->h_addr_list[i]; i++){
        strcpy(ip_addr, inet_ntoa( * (struct in_addr*) host->h_addr_list[i]));
        log_e("get host from host_name, ip_addr:%s\n", ip_addr);
        break;
    }
}

void progress_bar(long cur_size, long total_size, double speed)
{
    /*用于显示下载进度条*/
    float percent = (float) cur_size / total_size;
    const int numTotal = 50;
    int numShow = (int)(numTotal * percent);

    if (numShow == 0){
        numShow = 1;
    }

    if (numShow > numTotal){
        numShow = numTotal;
    }

    char sign[51] = {0};
    memset(sign, '=', numTotal);

    printf("\r%.2f%%[%-*.*s] %.2f/%.2fMB %4.0fkb/s", percent * 100, numTotal, numShow, sign, cur_size / 1024.0 / 1024.0, total_size / 1024.0 / 1024.0, speed);
    fflush(stdout);

    if (numShow == numTotal){
        log_d("\n");
    }
}

unsigned long get_file_size(const char *filename)
{
    log_d("get_file_size\n");
    //通过系统调用直接得到文件的大小
    struct stat buf;
    if (stat(filename, &buf) < 0){
        return 0;
    }
    return (unsigned long) buf.st_size;
}

void download(int client_socket)
{
    log_d("download\n");
    /*下载文件函数*/
    unsigned long hasrecieve = 0;//记录已经下载的长度
    struct timeval t_start, t_end;//记录一次读取的时间起点和终点, 计算速度
    int mem_size = 8192;//缓冲区大小8K
    int buf_len = mem_size;//理想状态每次读取8K大小的字节流
    int len;
    log_d("start download file:%s\n", full_http_res_header.file_name);
    int fd = open(full_http_res_header.file_name, O_CREAT | O_WRONLY, S_IRWXG | S_IRWXO | S_IRWXU);//创建文件描述符
    char *buf = (char *) malloc(mem_size * sizeof(char));
    //从套接字流中读取文件流
    long diff = 0;
    int prelen = 0;
    double speed;
    
    if (fd < 0){
        log_e("文件创建失败!\n");
        exit(0);
    }
    
    while (hasrecieve < full_http_res_header.content_length){
        gettimeofday(&t_start, NULL ); //获取开始时间
        len = read(client_socket, buf, buf_len);
        write(fd, buf, len);
        gettimeofday(&t_end, NULL ); //获取结束时间
        hasrecieve += len;//更新已经下载的长度
        //计算速度
        if (t_end.tv_usec - t_start.tv_usec >= 0 &&  t_end.tv_sec - t_start.tv_sec >= 0){
            diff += 1000000 * ( t_end.tv_sec - t_start.tv_sec ) + (t_end.tv_usec - t_start.tv_usec);//us
        }
        //当一个时间段大于1s=1000000us时, 计算一次速度
        if (diff >= 1000000){
            speed = (double)(hasrecieve - prelen) / (double)diff * (1000000.0 / 1024.0);
            prelen = hasrecieve;//清零下载量
            diff = 0;//清零时间段长度
        }
        progress_bar(hasrecieve, full_http_res_header.content_length, speed);
        if (hasrecieve == full_http_res_header.content_length){
            break;
        }
    }
}

void continue_download(int client_socket)
{
    log_d("continue_download\n");
    /*下载文件函数*/
    unsigned long hasrecieve = 0;//记录已经下载的长度
    struct timeval t_start, t_end;//记录一次读取的时间起点和终点, 计算速度
    int mem_size = 8192;//缓冲区大小8K
    int buf_len = mem_size;//理想状态每次读取8K大小的字节流
    int len;

    //创建文件描述符
    log_d("start continue download file:%s\n", tmp_http_res_header.file_name);
    int fd = open(tmp_http_res_header.file_name, O_APPEND, S_IRWXG | S_IRWXO | S_IRWXU);
    if (fd < 0){
        log_e("文件创建失败!\n");
        exit(0);
    }

    char *buf = (char *) malloc(mem_size * sizeof(char));

    //从套接字流中读取文件流
    long diff = 0;
    int prelen = 0;
    double speed;

    while (hasrecieve < tmp_http_res_header.content_length){
        gettimeofday(&t_start, NULL ); //获取开始时间
        len = read(client_socket, buf, buf_len);
        write(fd, buf, len);
        gettimeofday(&t_end, NULL ); //获取结束时间
        hasrecieve += len;//更新已经下载的长度

        //计算速度
        if (t_end.tv_usec - t_start.tv_usec >= 0 &&  t_end.tv_sec - t_start.tv_sec >= 0){
            diff += 1000000 * ( t_end.tv_sec - t_start.tv_sec ) + (t_end.tv_usec - t_start.tv_usec);//us
        }
        //当一个时间段大于1s=1000000us时, 计算一次速度
        if (diff >= 1000000){
            speed = (double)(hasrecieve - prelen) / (double)diff * (1000000.0 / 1024.0);
            prelen = hasrecieve;//清零下载量
            diff = 0;//清零时间段长度
        }
        progress_bar(hasrecieve, tmp_http_res_header.content_length, speed);
        if (hasrecieve == tmp_http_res_header.content_length){
            break;
        }
    }
}

int check_download_file()
{
    log_d("check_download_file\n");
    int ret = 0;
    full_size = get_file_size(full_http_res_header.file_name);
    if (full_http_res_header.content_length == full_size){
        ret = SUCCESS_OK;
        log_d("\n文件%s下载成功!\n\n", full_http_res_header.file_name);
    }else if(full_http_res_header.content_length > full_size){
        ret = ERROR_DOWNLOAD_FAILED;
        log_e("\n文件下载中有字节缺失, 下载失败, 请重试! content_length:%ld, file size:%ld\n\n", full_http_res_header.content_length, full_size);
    }else{
        ret = ERROR_DOWNLOAD_FAILED;
        log_e("\n文件%s下载未完成! 已下:%ld, total:%ld\n\n", full_http_res_header.file_name, full_size, full_http_res_header.content_length);
    }
    return ret;
}

//http请求头信息
int send_http_header(int client_socket)
{
    log_d("send_http_header\n");
    char header[2048] = {0};
    
    if(send_again){
        tmp_size = get_file_size(tmp_http_res_header.file_name);
        if(tmp_size > 0){
            sprintf(header, \
                "GET %s HTTP/1.1\r\n"\
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"\
                "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537(KHTML, like Gecko) Chrome/47.0.2526Safari/537.36\r\n"\
                "Host: %s\r\n"\
                "Range: bytes=%ld-\r\n"\
                "Connection: keep-alive\r\n"\
                "\r\n", host_info.url, host_info.host, tmp_size);
            
        }
    }else{
        sprintf(header, \
            "GET %s HTTP/1.1\r\n"\
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"\
            "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537(KHTML, like Gecko) Chrome/47.0.2526Safari/537.36\r\n"\
            "Host: %s\r\n"\
            "Connection: keep-alive\r\n"\
            "\r\n", host_info.url, host_info.host);
    }
    log_d("Tmp_Header\n\n%s", header);
    return write(client_socket, header, strlen(header));
}

void get_http_response(int client_socket)
{
    log_d("get_http_response\n");
    int mem_size = 4096;
    int length, len = 0;
    char *buf = (char *) malloc(mem_size * sizeof(char));
    char *response = (char *) malloc(mem_size * sizeof(char));

    //每次单个字符读取响应头信息
    while ((len = read(client_socket, buf, 1)) != 0){
        //log_d("get_http_response running in while len:%d, length:%d\n", len, length);
        if (length + len > mem_size){
            //动态内存申请, 因为无法确定响应头内容长度
            mem_size *= 2;
            char * temp = (char *) realloc(response, sizeof(char) * mem_size);
            if (temp == NULL){
                log_e("\t动态内存申请失败\n");
                exit(-1);
            }
            response = temp;
        }

        buf[len] = '\0';
        strcat(response, buf);

        //找到响应头的头部信息
        int i,flag = 0;
        for (i = strlen(response) - 1; response[i] == '\n' || response[i] == '\r'; i--, flag++);
        //连续两个换行和回车表示已经到达响应头的头尾, 即将出现的就是需要下载的内容
        if (flag == 4){
            break;
        }
        length += len;
    }
    
    log_d("get_http_response out of while\n");
    parse_header(response);
}

int parse_http_header(int client_socket, HTTP_RES_HEADER *http_res_header)
{
    log_d("parse_http_header\n");
    char buffer[BUFSIZ],temp[BUFSIZ],*ptr;
    bzero(buffer,sizeof(buffer));
    bzero(temp,sizeof(temp));
    int len,n = 0;
    while((len = read(client_socket,buffer,1)) != 0){
        temp[n] = *buffer;
        if (*buffer == '\n'){
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
            //printf("%s",temp);
            if (temp[0] == '\r' && temp[1] == '\n')
                break;
            bzero(temp,sizeof(temp));
            n = -1;
        }
        n++;
    }
    log_d(">>>>http响应头解析成功:<<<<\n\n");
    log_d("http_res_header Status-Code: %d\n", http_res_header->status_code);
    log_d("http_res_header Content-Type: %s\n", http_res_header->content_type);
    log_d("http_res_header Content-Range: %s\n", http_res_header->content_range);
    log_d("http_res_header Content-Length: %ld bytes\n\n", http_res_header->content_length);
    log_d("http_res_header FileName: %s \n", http_res_header->file_name);
    log_d("http_res_header IPAddress: %s\n\n", http_res_header->ip_addr);
    
    return client_socket;
}

int http_get_full()
{
    int ret = 0;
    log_d("http_get_full\n");
    log_d("create socket...\n");
    int client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket < 0){
        ret = ERROR_CREATE_SOCKET_FAILED;
        log_e("\tcreate socket failed: %d\n", client_socket);
        return ret;
    }
    
    //连接远程主机
    log_d("connect to host...\n");
    int res = connect(client_socket, (struct sockaddr *) &addr, sizeof(addr));
    if (res == -1){
        log_e("\tcreate connect failed, error: %d\n", res);
        ret = -1;
        return ret;
    }else{
        ret = send_http_header(client_socket);
    }
    
    if(ret == -1){
        log_e("\t发送http报文失败: %d\n", ret);
        return ERROR_SEND_HEADER_FAILED;
    }else{
        log_e("\t发送http报文成功: %d\n", ret);
        ret = parse_http_header(client_socket, &full_http_res_header);
        //get_http_response(client_socket);
        
        if (full_http_res_header.status_code != 200){
            log_e("\t文件无法下载, 远程主机返回: %d\n", full_http_res_header.status_code);
            ret = ERROR_HTTP_STATUS_CODE;
        }else if(!file_exist){
            send_again = 0;
            log_d("downloading...\n");
            download(client_socket);
            ret = check_download_file();
        }else{
            send_again = 1;
        }
        close(client_socket);
    }
    
    return ret;
}

int http_get_tmp()
{
    int ret = 0;
    log_d("http_get_tmp\n");
    log_d("create socket...\n");
    int client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket < 0){
        ret = ERROR_CREATE_SOCKET_FAILED;
        log_e("\tcreate socket failed: %d\n", client_socket);
        return ret;
    }
    
    //连接远程主机
    log_d("connect to host...\n");
    int res = connect(client_socket, (struct sockaddr *) &addr, sizeof(addr));
    if (res == -1){
        log_e("\tcreate connect failed, error: %d\n", res);
        ret = -1;
        return ret;
    }else{
        ret = send_http_header(client_socket);
    }
    
    if(ret == -1){
        log_e("\t发送http报文失败: %d\n", ret);
        return ERROR_SEND_HEADER_FAILED;
    }else{
        log_e("\t发送http报文成功: %d\n", ret);
        ret = parse_http_header(client_socket, &tmp_http_res_header);
        //get_http_response(client_socket);
        
        if (tmp_http_res_header.status_code != 206){
            log_e("\t文件无法下载, 远程主机返回: %d\n", tmp_http_res_header.status_code);
            ret = ERROR_HTTP_STATUS_CODE;
        }else{
            log_d("downloading...\n");
            continue_download(client_socket);
            ret = check_download_file();
        }
        close(client_socket);
    }
    
    return ret;
}

int get_host_info()
{
    log_d("get_host_info\n");
    parse_url(host_info.url, host_info.host, &host_info.port, full_http_res_header.file_name);
    get_ip_addr(host_info.host, host_info.ip_addr);         //从url中分析出主机名, 端口号, 文件名
    strcpy(full_http_res_header.ip_addr, host_info.ip_addr);
    strcpy(tmp_http_res_header.file_name, full_http_res_header.file_name);
    strcpy(tmp_http_res_header.ip_addr, full_http_res_header.ip_addr);
    
    if (strlen(host_info.ip_addr) == 0){
        log_e(">>>>下载地址解析失败<<<<\n");
        return ERROR_PARSE_URL;
    }else{
        log_d(">>>>下载地址解析成功<<<<\n");
        log_d(" URL: %s\n", host_info.url);
        log_d(" Host: %s\n", host_info.host);
        log_d(" IP Address: %s\n", host_info.ip_addr);
        log_d(" Port: %d\n", host_info.port);
        log_d(" FileName : %s\n", full_http_res_header.file_name);
        
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = inet_addr(host_info.ip_addr);
        addr.sin_port = htons(host_info.port);
        
        if(!access(full_http_res_header.file_name,0)){
            file_exist = 1;
            log_d("file exist\n");
        }else{
            file_exist = 0;
            log_d("file not exist\n");
        }
        
        return 1;
    }
}

void init()
{
    log_d("====init====\n");
    bzero(&host_info,sizeof(host_info));
    bzero(&full_http_res_header,sizeof(full_http_res_header));
    bzero(&tmp_http_res_header,sizeof(tmp_http_res_header));
    bzero(&addr,sizeof(addr));
    file_exist = 0;
    send_again = 0;
}

void free_all()
{
    log_d("====free_all====\n");
    bzero(&host_info,sizeof(host_info));
    bzero(&full_http_res_header,sizeof(full_http_res_header));
    bzero(&tmp_http_res_header,sizeof(tmp_http_res_header));
    bzero(&addr,sizeof(addr));
}

int main(int argc, char const *argv[])
{
    int ret = 0;
    init();
    
    if (argc == 1){
        log_e("您必须给定一个http地址才能开始工作\n");
        ret = ERROR_INPUT_IP;
        return ret;
    }else if(argc == 2){
        strcpy(host_info.url, argv[1]);
        log_d("input url : %s\n", host_info.url);
    }else if(argc == 3){
        strcpy(host_info.url, argv[1]);
        strcpy(host_info.new_file_name, argv[2]);
        log_d("input url : %s, new name : %s\n", host_info.url, host_info.new_file_name);
    }
    
    ret = get_host_info();
    if(ret == ERROR_PARSE_URL){
        log_e("\t错误: 无法获取到远程服务器的IP地址, 请检查下载地址的有效性\n");
        return ret;
    }
    
    
    ret = http_get_full();
    if(send_again){
        ret = http_get_tmp();
    }
    
    free_all();
    return ret;
}

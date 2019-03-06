#include "include/fota.h"

HOST_INFO host_info;
HTTP_RES_HEADER tmp_http_res_header;
HTTP_RES_HEADER full_http_res_header;

void parse_url(const char *url, char *host, int *port, char *file_name)
{
    /*通过url解析出域名, 端口, 以及文件名*/
    int i,j = 0;
    int start = 0;
    *port = 80;
    char *patterns[] = {"http://", "https://", NULL};

    for (i = 0; patterns[i]; i++)//分离下载地址中的http协议
    {
        if (strncmp(url, patterns[i], strlen(patterns[i])) == 0)
        {
            start = strlen(patterns[i]);
        }
    }

    //解析域名, 这里处理时域名后面的端口号会保留
    i = 0;
    for (i = start; url[i] != '/' && url[i] != '\0'; i++, j++)
    {
        host[j] = url[i];
    }
    host[j] = '\0';

    //解析端口号, 如果没有, 那么设置端口为80
    char *pos = strstr(host, ":");
    if (pos)
    {
        sscanf(pos, ":%d", port);
    }

    //删除域名端口号
    i = 0;
    for (i = 0; i < (int)strlen(host); i++)
    {
        if (host[i] == ':')
        {
            host[i] = '\0';
            break;
        }
    }

    //获取下载文件名
    j = 0;
    i = 0;
    for (i = start; url[i] != '\0'; i++)
    {
        if (url[i] == '/')
        {
            if (i !=  strlen(url) - 1)
            {
                j = 0;
            }
            continue;
        }
        else
        {
            file_name[j++] = url[i];
        }
    }
    file_name[j] = '\0';
}

struct HTTP_RES_HEADER parse_header(const char *response)
{
    /*获取响应头的信息*/
    struct HTTP_RES_HEADER resp;

    char *pos = strstr(response, "HTTP/");
    if (pos)//获取返回代码
    {
        sscanf(pos, "%*s %d", &resp.status_code);
    }

    pos = strstr(response, "Content-Type:");
    if (pos)//获取返回文档类型
    {
        sscanf(pos, "%*s %s", resp.content_type);
    }

    pos = strstr(response, "Content-Range:");
    if (pos)//获取返回文档起始点
    {
        sscanf(pos, "%*s %s", resp.content_range);
    }

    pos = strstr(response, "Accept-Ranges:");
    if (pos)//获取返回文档单位
    {
        sscanf(pos, "%*s %s", resp.accept_ranges);
    }

    pos = strstr(response, "Content-Length:");
    if (pos)//获取返回文档长度
    {
        sscanf(pos, "%*s %ld", &resp.content_length);
    }

    return resp;
}

void get_ip_addr(char *host_name, char *ip_addr)
{
    /*通过域名得到相应的ip地址*/
    int i = 0;
    struct hostent *host = gethostbyname(host_name);//此函数将会访问DNS服务器
    if (!host)
    {
        ip_addr = NULL;
        return;
    }

    for (i = 0; host->h_addr_list[i]; i++)
    {
        strcpy(ip_addr, inet_ntoa( * (struct in_addr*) host->h_addr_list[i]));
        break;
    }
}

void progress_bar(long cur_size, long total_size, double speed)
{
    /*用于显示下载进度条*/
    float percent = (float) cur_size / total_size;
    const int numTotal = 50;
    int numShow = (int)(numTotal * percent);

    if (numShow == 0)
    {
        numShow = 1;
    }

    if (numShow > numTotal)
    {
        numShow = numTotal;
    }

    char sign[51] = {0};
    memset(sign, '=', numTotal);

    log_d("\r%.2f%%[%-*.*s] %.2f/%.2fMB %4.0fkb/s", percent * 100, numTotal, numShow, sign, cur_size / 1024.0 / 1024.0, total_size / 1024.0 / 1024.0, speed);
    fflush(stdout);

    if (numShow == numTotal)
    {
        log_d("\n");
    }
}

unsigned long get_file_size(const char *filename)
{
    //通过系统调用直接得到文件的大小
    struct stat buf;
    if (stat(filename, &buf) < 0)
    {
        return 0;
    }
    return (unsigned long) buf.st_size;
}

void download(int client_socket, char *file_name, unsigned long content_length)
{
    /*下载文件函数*/
    unsigned long hasrecieve = 0;//记录已经下载的长度
    struct timeval t_start, t_end;//记录一次读取的时间起点和终点, 计算速度
    int mem_size = 8192;//缓冲区大小8K
    int buf_len = mem_size;//理想状态每次读取8K大小的字节流
    int len;

    //创建文件描述符
    int fd = open(file_name, O_CREAT | O_WRONLY, S_IRWXG | S_IRWXO | S_IRWXU);
    if (fd < 0)
    {
        log_d("文件创建失败!\n");
        exit(0);
    }

    char *buf = (char *) malloc(mem_size * sizeof(char));

    //从套接字流中读取文件流
    long diff = 0;
    int prelen = 0;
    double speed;

    while (hasrecieve < content_length)
    {
        gettimeofday(&t_start, NULL ); //获取开始时间
        len = read(client_socket, buf, buf_len);
        write(fd, buf, len);
        gettimeofday(&t_end, NULL ); //获取结束时间

        hasrecieve += len;//更新已经下载的长度

        //计算速度
        if (t_end.tv_usec - t_start.tv_usec >= 0 &&  t_end.tv_sec - t_start.tv_sec >= 0)
        {
            diff += 1000000 * ( t_end.tv_sec - t_start.tv_sec ) + (t_end.tv_usec - t_start.tv_usec);//us
        }

        if (diff >= 1000000)//当一个时间段大于1s=1000000us时, 计算一次速度
        {
            speed = (double)(hasrecieve - prelen) / (double)diff * (1000000.0 / 1024.0);
            prelen = hasrecieve;//清零下载量
            diff = 0;//清零时间段长度
        }

        progress_bar(hasrecieve, content_length, speed);

        if (hasrecieve == content_length)
        {
            break;
        }
    }
}

void continue_download(int client_socket, char *file_name, unsigned long content_length)
{
    /*下载文件函数*/
    unsigned long hasrecieve = 0;//记录已经下载的长度
    struct timeval t_start, t_end;//记录一次读取的时间起点和终点, 计算速度
    int mem_size = 8192;//缓冲区大小8K
    int buf_len = mem_size;//理想状态每次读取8K大小的字节流
    int len;

    //创建文件描述符
    int fd = open(file_name, O_APPEND, S_IRWXG | S_IRWXO | S_IRWXU);
    if (fd < 0)
    {
        log_e("文件创建失败!\n");
        exit(0);
    }

    char *buf = (char *) malloc(mem_size * sizeof(char));

    //从套接字流中读取文件流
    long diff = 0;
    int prelen = 0;
    double speed;

    while (hasrecieve < content_length)
    {
        gettimeofday(&t_start, NULL ); //获取开始时间
        len = read(client_socket, buf, buf_len);
        write(fd, buf, len);
        gettimeofday(&t_end, NULL ); //获取结束时间

        hasrecieve += len;//更新已经下载的长度

        //计算速度
        if (t_end.tv_usec - t_start.tv_usec >= 0 &&  t_end.tv_sec - t_start.tv_sec >= 0)
        {
            diff += 1000000 * ( t_end.tv_sec - t_start.tv_sec ) + (t_end.tv_usec - t_start.tv_usec);//us
        }

        if (diff >= 1000000)//当一个时间段大于1s=1000000us时, 计算一次速度
        {
            speed = (double)(hasrecieve - prelen) / (double)diff * (1000000.0 / 1024.0);
            prelen = hasrecieve;//清零下载量
            diff = 0;//清零时间段长度
        }

        progress_bar(hasrecieve, content_length, speed);

        if (hasrecieve == content_length)
        {
            break;
        }
    }
}

//http请求头信息
int send_http_header(int client_socket)
{
    char header[2048] = {0};
    
    if(!access(http_res_header.file_name,0))
    {
        file_exist = 1;
        tmp_size = get_file_size(http_res_header.file_name);
        log_d("文件已存在, tmp size : %ld\n", tmp_size);
        if(tmp_size > 0)
        {
            sprintf(header, \
                "GET %s HTTP/1.1\r\n"\
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"\
                "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537(KHTML, like Gecko) Chrome/47.0.2526Safari/537.36\r\n"\
                "Host: %s\r\n"\
                "Range: bytes=%ld-\r\n"\
                "Connection: keep-alive\r\n"\
                "\r\n"\
            ,url, host, tmp_size);
            log_d("\nTmp_Header\n%s", header);
        }
    }
    else
    {
        file_exist = -1;
        log_d("文件不存在\n");
        sprintf(header, \
            "GET %s HTTP/1.1\r\n"\
            "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"\
            "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537(KHTML, like Gecko) Chrome/47.0.2526Safari/537.36\r\n"\
            "Host: %s\r\n"\
            "Connection: keep-alive\r\n"\
            "\r\n"\
        ,url, host);
        log_d("\nFull_Header\n%s", header);
    }
    
    return write(client_socket, header, strlen(header));
}

int http_get(int client_socket, struct sockaddr_in addr)
{
    int ret = 0;
    //连接远程主机
    log_d("connect to host...\n");
    int res = connect(client_socket, (struct sockaddr *) &addr, sizeof(addr));
    if (res == -1)
    {
        log_e("\t连接远程主机失败, error: %d\n", res);
        ret = -1;
        return ret;
    }
    
    //发送http报文
    ret = send_http_header(client_socket);
    if(ret == -1)
    {
        log_e("\t发送http报文失败: %d\n", ret);
        return ERROR_SEND_HEADER_FAILED;
    }

    int mem_size = 4096;
    int length = 0;
    int len;
    char *buf = (char *) malloc(mem_size * sizeof(char));
    char *response = (char *) malloc(mem_size * sizeof(char));

    //每次单个字符读取响应头信息
    log_d("6: 正在解析http响应头...\n");
    while ((len = read(client_socket, buf, 1)) != 0)
    {
        if (length + len > mem_size)
        {
            //动态内存申请, 因为无法确定响应头内容长度
            mem_size *= 2;
            char * temp = (char *) realloc(response, sizeof(char) * mem_size);
            if (temp == NULL)
            {
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
        if (flag == 4)//连续两个换行和回车表示已经到达响应头的头尾, 即将出现的就是需要下载的内容
        {
            break;
        }
        length += len;
    }

    struct HTTP_RES_HEADER resp = parse_header(response);

    log_d(">>>>http响应头解析成功:<<<<\n\n");

    log_d(" Status-Code: %d\n", resp.status_code);
    log_d(" Content-Type: %s\n", resp.content_type);
    log_d(" Accept-Ranges: %s\n", resp.accept_ranges);
    log_d(" Content-Range: %s\n", resp.content_range);
    log_d(" Content-Length: %ld bytes\n\n", resp.content_length);
    
    if (resp.status_code != 200 && resp.status_code != 206)
    {
        log_e("\t文件无法下载, 远程主机返回: %d\n", resp.status_code);
        return 0;
    }
    else
    {
        log_d("7: 开始文件下载...\n");
        if(resp.status_code == 200)
        {
            download(client_socket, file_name, resp.content_length);
        }
        else if(resp.status_code == 206)
        {
            continue_download(client_socket, file_name, resp.content_length);
        }
        file_size = get_file_size(file_name);
        if (resp.content_length == file_size)
        {
            log_d("\n文件%s下载成功! ^_^\n\n", file_name);
        }
        else if(resp.content_length > file_size)
        {
            //remove(file_name);
            log_e("\n文件下载中有字节缺失, 下载失败, 请重试! content_length:%ld, file size:%ld\n\n", resp.content_length, file_size);
        }
        else
        {
            log_e("\n文件%s下载未完成! 已下:%ld, total:%ld\n\n", file_name, file_size, resp.content_length);
        }
    }
}

int init()
{
    log_d("init host_info, http_res_header\n");
    bzero(&host_info,sizeof(host_info));
    bzero(&full_http_res_header,sizeof(full_http_res_header));
    bzero(&tmp_http_res_header,sizeof(tmp_http_res_header));
    
    parse_url(host_info.url, host_info.host, &host_info.port, full_http_res_header.file_name);
    get_ip_addr(host_info.host, host_info.ip_addr);         //从url中分析出主机名, 端口号, 文件名
    full_http_res_header.ip_addr = host_info.ip_addr;        //调用函数同访问DNS服务器获取远程主机的IP
    tmp_http_res_header.file_name = full_http_res_header.file_name;
    tmp_http_res_header.ip_addr = full_http_res_header.ip_addr;
    
    if (strlen(host_info.ip_addr) == 0)
    {
        log_e(">>>>下载地址解析失败<<<<\n");
        return ERROR_PARSE_URL;
    }
    else
    {
        log_d(">>>>下载地址解析成功<<<<\n");
        log_d(" URL: %s\n", host_info.url);
        log_d(" Host: %s\n", host_info.host);
        log_d(" IP Address: %s\n", host_info.ip_addr);
        log_d(" Port: %d\n", host_info.port);
        log_d(" File Name : %s\n\n", full_http_res_header.file_name);
        return 1;
    }
}

int main(int argc, char const *argv[])
{
    int ret = 0;
    
    if (argc == 1)
    {
        log_e("您必须给定一个http地址才能开始工作\n");
        ret = ERROR_INPUT_IP;
        return ret;
    }
    else
    {
        strcpy(host_info.url, argv[1]);
    }
    
    ret = init();
    if(ret == ERROR_PARSE_URL)
    {
        log_e("\t错误: 无法获取到远程服务器的IP地址, 请检查下载地址的有效性\n");
        return ret;
    }
    
    log_d("create socket...\n");
    int client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket < 0)
    {
        ret = ERROR_CREATE_SOCKET_FAILED;
        log_e("\tcreate socket failed: %d\n", client_socket);
        return ret;
    }
    
    ret = http_get(client_socket, addr, full_header);
    
    //创建IP地址结构体
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host_info.ip_addr);
    addr.sin_port = htons(host_info.port);
    
    log_d("8: 关闭套接字\n");
    shutdown(client_socket, 2);//关闭套接字的接收和发送
    return ret;
}

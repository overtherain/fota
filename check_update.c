#include "check_update.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <assert.h>
#include "cutils/android_reboot.h"
#include "cutils/properties.h"
#include "get_update_file.h"

//#define PROPERTY_VALUE_MAX 2048

#ifdef __cplusplus
extern "C"
{
#endif

pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t cond_mutex = PTHREAD_MUTEX_INITIALIZER;
int is_check_update = 1;
HOST_INFO   check_host_info;
HOST_INFO   download_host_info;
HTTP_RESPONSE_HEADER    http_response_header;

char* change(char* str)
{
    assert(str != NULL);//断言，判断str知否指向空 
    char* pstr = str;//由于str要进行变化，以后还要用到，所以先把他用pstr存起来 
    int space_count = 0;//计数器 
    char* end = NULL;
    char* end_new = NULL;
    while(*str++!='\0')
    {
        if(*str == ' ')
            space_count++;//进行空格计数 
    }
    end = str;
    end_new = end + 2*space_count;
    str = pstr;
    while(end != end_new)//当新结束指针和原结束指针不相等时 
    {
        if(*end == ' ')//发现空格，变为“%20” 
        {
            *end_new-- = '0';
            *end_new-- = '2';
            *end_new-- = '%';
            end--;
        }
        else//否则进行赋值 
        {
            *end_new-- = *end--;
        }
    }
    return pstr;//将变化后的字符串的首地址返回 
}


int space_change(char *arr1,char *arr2)
{
    while (*arr1)
    {
        if (*arr1 != ' ')
        {
            *arr2 = *arr1;
            arr2++;
        }
        else
        {
            strcpy(arr2, "%20");
                arr2 = arr2 + 3;
        }
        arr1++;
    }
    *arr2 = '\0';
    return 0;
}

int encode_result(char *arr1, char *arr2, char *bstr, char *estr)
{
    char *p1, *p2;
    p1 = strstr(arr1, bstr);
    p2 = strstr(arr1, estr);
    if(p1 == NULL || p2 == NULL || p1 > p2){
        log_e("encode_result Notfound '%s', '%s'\n",bstr, estr);
        return 0;
    }else{
        p1 += strlen(bstr);
        memcpy(arr2, p1, p2 - p1);
    }
    return 1;
}

int cut_str(char *arr1, char *arr2)
{
    char *p1;
    int p2 = 0;
    p1 = strstr(arr1, ":");
    p2 = strlen(arr1);
    log_d("cut_str arr1 length : %d\n", p2);
    if(p1 == NULL || p2 == 0){
        log_e("cut_str error!\n");
        return 0;
    }else{
        p1 += 2;
        memcpy(arr2, p1, strlen(p1)-1);
    }
    return 1;
    
}

void *check_server_thread(void *arg)
{
    int ret = 0;
    char url[2048], convert_url[2048];
    char hardware[PROPERTY_VALUE_MAX];
    char hardware_version[PROPERTY_VALUE_MAX];
    char software_version[PROPERTY_VALUE_MAX];
    char serialno[PROPERTY_VALUE_MAX];
    char model[PROPERTY_VALUE_MAX];
    char imei[PROPERTY_VALUE_MAX];
    HOST_INFO check_host_info;
    HTTP_RESPONSE_HEADER http_response_header;
    struct timespec timeout;
    int socketfd;
    
    char check_update_result[64];
    char check_update_hardware[64];
    char check_update_model[64];
    char check_update_file_name[64];
    char check_update_file_path[64];
    char check_update_file_download_url[2048];
    char check_update_file_size[64];
    char check_update_description[2048];
    char check_update_srcVersion[1024];
    char check_update_dstVersion[1024];
    char check_update_file_md5[64];
    is_check_update = 1;
    
    while(is_check_update){
        bzero(&check_host_info,sizeof(check_host_info));
        bzero(&download_host_info,sizeof(download_host_info));
        bzero(&http_response_header,sizeof(http_response_header));
        
        property_get("ro.product.hardware", hardware, "");
        log_d("check_server_thread hardware:%s\n", hardware);
        
        property_get("ro.product.device", hardware_version, "");
        log_d("check_server_thread hardware_version:%s\n", hardware_version);
        
        property_get("ro.product.dfsl_version", software_version, "");
        log_d("check_server_thread software_version:%s\n", software_version);
        
        property_get("ro.boot.serialno", serialno, "");
        log_d("check_server_thread serialno:%s\n", serialno);
        
        property_get("ro.product.model", model, "");
        log_d("check_server_thread model:%s\n", model);
        
        sprintf(url, "http://121.43.183.196:8081/updsvr/ota/checkupdate?hw=%s&hwv=%s&swv=%s&serialno=%s&model=%s",
                hardware, hardware_version, software_version, serialno, model);
        
        log_d("check_server_thread url:%s\n", url);
        space_change(url, convert_url);
        log_d("check_server_thread convert_url:%s\n", convert_url);
        
        if(strlen(url) != 0){
            strcpy(check_host_info.url, convert_url);
            parse_url(check_host_info.url, check_host_info.host, &check_host_info.port, http_response_header.file_name);
            get_ip_addr(check_host_info.host, check_host_info.ip_addr);
            if (strlen(check_host_info.ip_addr) == 0){
                log_e("parse_url failed\n");
                return (void *)ERROR_PARSE_URL;
            }else{
                log_d("parse_url success\n");
                log_d(" URL: %s\n", check_host_info.url);
                log_d(" Host: %s\n", check_host_info.host);
                log_d(" IP Address: %s\n", check_host_info.ip_addr);
                log_d(" Port: %d\n", check_host_info.port);
                log_d(" FileName : %s\n", http_response_header.file_name);
            }
            socketfd = socket(AF_INET, SOCK_STREAM, 0);
            if (socketfd < 0){
                log_e("\tcreate socket failed: %d\n", socketfd);
                return (void *)ERROR_CREATE_SOCKET_FAILED;
            }else{
                log_e("\tcreate socket success: %d\n", socketfd);
            }
            
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr(check_host_info.ip_addr);
            addr.sin_port = htons(check_host_info.port);
            
            log_d("connect to host...\n");
            ret = connect(socketfd, (struct sockaddr *) &addr, sizeof(addr));
            if (ret == -1){
                log_e("\tcreate connect failed, error: %d\n", ret);
                return (void *)ERROR_CONNECT_SOCKET_FAILED;
            }else{
                log_e("\tcreate connect success, res: %d\n", ret);
            }
            ret = send_http_header(socketfd, 0, &check_host_info);
            if(ret == -1){
                log_e("\tsend_http_header failed: %d\n", ret);
                close(socketfd);
                return (void *)ERROR_SEND_HEADER_FAILED;
            }else{
                log_d("send_http_header success: %d\n", ret);
                
                ret = parse_http_response_header(socketfd, &http_response_header);
                /*log_d("http_response_header Status-Code: %d\n", http_response_header.status_code);
                log_d("http_response_header Content-Type: %s\n", http_response_header.content_type);
                log_d("http_response_header Content-Range: %s\n", http_response_header.content_range);
                log_d("http_response_header Content-Length: %ld bytes\n", http_response_header.content_length);
                log_d("http_response_header FileName: %s \n", http_response_header.file_name);
                log_d("http_response_header Transfer-Encoding : %s\n", http_response_header.transfer_encoding);*/
                
                if(http_response_header.status_code == 200){
                    log_d("get reponse success \n\n");
                    char buffer[BUFSIZ], tmp[BUFSIZ], json_buffer[BUFSIZ] = { 0 };
                    int len = 0;
                    while((len = read(socketfd, tmp, sizeof(tmp))) > 0){
                        log_d("get reponse tmp : '%s'\n", tmp);
                        if(strstr(tmp, "0\r\n") != NULL){
                            break;
                        }else{
                            strcpy(buffer, tmp);
                        }
                        bzero(&tmp,sizeof(tmp));
                    }
                    if(strlen(buffer) != 0){
                        encode_result(buffer, json_buffer, "{", "}");
                        log_d("get reponse end json_buffer : \n'%s'\n", json_buffer);
                        
                        char *token = strtok(json_buffer, ",");
                        while(token){
                            //log_d("get items : %s\n", token);
                            char *ret;
                            if((ret = strstr(token, "CHECK_UPDATE_RESULT")) != NULL){
                                cut_str(ret, check_update_result);
                                log_d("check_update_result %s\n", check_update_result);
                            }else if((ret = strstr(token, "hardware")) != NULL){
                                cut_str(ret, check_update_hardware);
                                log_d("check_update_hardware %s\n", check_update_hardware);
                            }else if((ret = strstr(token, "model")) != NULL){
                                cut_str(ret, check_update_model);
                                log_d("check_update_model %s\n", check_update_model);
                            }else if((ret = strstr(token, "fileName")) != NULL){
                                cut_str(ret, check_update_file_name);
                                log_d("check_update_file_name %s\n", check_update_file_name);
                            }else if((ret = strstr(token, "filePath")) != NULL){
                                cut_str(ret, check_update_file_path);
                                log_d("check_update_file_path %s\n", check_update_file_path);
                            }else if((ret = strstr(token, "downloadUrl")) != NULL){
                                cut_str(ret, check_update_file_download_url);
                                log_d("check_update_file_download_url %s\n", check_update_file_download_url);
                            }else if((ret = strstr(token, "size")) != NULL){
                                cut_str(ret, check_update_file_size);
                                log_d("check_update_file_size %s\n", check_update_file_size);
                            }else if((ret = strstr(token, "description")) != NULL){
                                cut_str(ret, check_update_description);
                                log_d("check_update_description %s\n", check_update_description);
                            }else if((ret = strstr(token, "srcVersion")) != NULL){
                                cut_str(ret, check_update_srcVersion);
                                log_d("check_update_srcVersion %s\n", check_update_srcVersion);
                            }else if((ret = strstr(token, "dstVersion")) != NULL){
                                cut_str(ret, check_update_dstVersion);
                                log_d("check_update_dstVersion %s\n", check_update_dstVersion);
                            }else if((ret = strstr(token, "md5")) != NULL){
                                cut_str(ret, check_update_file_md5);
                                log_d("check_update_file_md5 %s\n", check_update_file_md5);
                            }
                            token=strtok(NULL,",");
                        }
                        
                        if(strcmp(check_update_result, "0") == 0 && strlen(check_update_file_download_url) != 0 && strlen(check_update_file_md5) != 0 && strcmp(check_update_srcVersion, software_version) == 0){
                            is_check_update = 0;
                            strcpy(download_host_info.url, check_update_file_download_url);
                            strcpy(download_host_info.md5, check_update_file_md5);
                            close(socketfd);
                            return (void *)SUCCESS_HAVE_UPDATE;
                        }else if(strlen(check_update_file_download_url) == 0){
                            log_e("check update failed check_update_file_download_url is null\n");
                        }else if(strlen(check_update_file_md5) == 0){
                            log_e("check update failed check_update_file_md5 is null\n");
                        }else if(strcmp(check_update_srcVersion, software_version) != 0){
                            log_e("cmd_server_init software_version is right target, phone:%s, server:%s\n", software_version, check_update_srcVersion);
                        }else{
                            log_e("check update failed ret %s\n", check_update_result);
                        }
                    }else{
                        log_e("get reponse items failed %d\n", ERROR_INVALID_URL_REPONSE_ITEMS);
                        close(socketfd);
                        return (void *)ERROR_INVALID_URL_REPONSE_ITEMS;
                    }
                }else{
                    log_d("get reponse status_code : %d\n", http_response_header.status_code);
                    close(socketfd);
                    return (void *)ERROR_INVALID_REPONSE_STATUS_CODE;
                }
            }
            sleep(1);
            close(socketfd);
        }else{
            log_d("create_socket ERROR_INVALID_URL\n");
            return (void *)ERROR_INVALID_URL;
        }
        timeout.tv_sec = time(NULL) + 10;
        timeout.tv_nsec = 0;
        pthread_mutex_lock(&cond_mutex);
        pthread_cond_timedwait(&cond, &cond_mutex, &timeout);
        pthread_mutex_unlock(&cond_mutex);
    }
    return (void *)ret;
}

void *download_server_thread(void *arg)
{
    int ret = -1;
    int is_download_success = 0;
    while(!is_download_success){
        ret = get_update_file(download_host_info.url, "");
        if(ret){
            is_download_success = 1;
            const char *cmd_file = "/data/cache/recovery/command";
            int fd;
            char update_setting[100] = "--update_package=/data/update.zip\n--locale=en-US";
            
            ret = mkdir("/data/cache/", S_IRWXU | S_IRWXG | S_IRWXO);
            if(-1 == ret && errno != EEXIST){
                log_e("mkdir /data/cache/ failed. errno is %d\n", errno);
                return (void *) -1;
            }
            ret = mkdir("/data/cache/recovery/", S_IRWXU | S_IRWXG | S_IRWXO);
            if (-1 == ret && (errno != EEXIST))
            {
                log_e("mkdir /data/cache/recovery/ failed. errno is %d" , errno);
                return (void *) -1;
            }
            fd = open("/data/cache/recovery/command", O_WRONLY | O_CREAT, 0777);
            if (fd >= 0)
            {
                ret = write(fd, update_setting, strlen(update_setting) + 1);
                sync();
                close(fd);
            }
            else
            {
                log_e("open /data/cache/recovery/command failed");
                return (void *) -1;
            }
            /*if(access("/data/cache/recovery", 0)){
                ret = mkdir("/data/cache/recovery", 0777);
            }else{
                log_d("download_server_thread exist '/data/cache/recovery' dir\n");
            }
            fp = fopen(cmd_file, "w");
            if(fp){
                ret = fwrite(&update_setting, strlen(update_setting), 1, fp);
            }else{
                log_d("download_server_thread open file failed\n");
                return (void *) -1;
            }*/
            if(ret){
                //fflush(fp);
                //fclose(fp);
                sleep(1);
                log_d("download_server_thread write cmd success\n");
                android_reboot(ANDROID_RB_RESTART2, 0, "recovery");
                //execl("/system/bin/sh", "sh", "-c", "reboot recovery", (char *)0);
            }else{
                log_d("download_server_thread write cmd failed\n");
                return (void *) -2;
            }
        }
    }
    return (void *)ret;
}

int cmd_server_init(void)
{
    log_d("cmd_server_init start\n");
    int ret, recv1, recv2 = 0;
    pthread_t check_td, download_td;
    
    while(1){
        ret = pthread_create(&check_td, NULL, check_server_thread, NULL);
        if(ret != 0){
            log_d("can't create check_td %d:\n", ret);
            return 0;
        }
        if(0){
            is_check_update = 0;
            pthread_cond_signal(&cond);
        }
        
        ret = pthread_join(check_td, &recv1);
        if (ret != 0){
            log_d("cannot join with check_server_thread, recv1:%d \n", recv1);
        }else{
            log_d("join with check_server_thread ret:%d, recv1:%d\n", ret, recv1);
            log_d("check update file url:%s, md5:%s\n", download_host_info.url, download_host_info.md5);
            if((int)SUCCESS_HAVE_UPDATE == recv1){
                ret = pthread_create(&download_td, NULL, download_server_thread, NULL);
                if(ret != 0){
                    log_d("can't create download_td %d:\n", ret);
                    return 0;
                }
                ret = pthread_join(download_td, &recv2);
                if(ret != 0){
                    log_d("cannot join with download_server_thread, recv2:%d \n", recv2);
                }else{
                    log_d("join with download_server_thread ret:%d, recv2:%d\n", ret, recv2);
                }
            }
        }
        sleep(10);
    }
    
    sleep(1);
    log_d("cmd_server_init end!\n");
    return 0;
}

#ifdef __cplusplus
}
#endif

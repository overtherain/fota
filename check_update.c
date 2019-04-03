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
#include "md5.h"

//#define PROPERTY_VALUE_MAX 2048

#ifdef __cplusplus
extern "C"
{
#endif

#define READ_DATA_SIZE      1024
#define MD5_SIZE            16
#define MD5_STR_LEN         (MD5_SIZE * 2)
#define OTA_PATCH_PATH      "/data/update.zip"
#define THREAD_TIMEOUT      60

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
        log_e("%s:%d Notfound '%s', '%s'\n", __FUNCTION__, __LINE__, bstr, estr);
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
    log_d("%s:%d arr1 length : %d\n", __FUNCTION__, __LINE__, p2);
    if(p1 == NULL || p2 == 0){
        log_e("%s:%d error!\n", __FUNCTION__, __LINE__);
        return 0;
    }else{
        p1 += 2;
        memcpy(arr2, p1, strlen(p1)-1);
    }
    log_d("%s:%d out str:'%s'\n", __FUNCTION__, __LINE__, arr2);
    return 1;
    
}

int compute_file_md5(const char *file_path, char *md5_str)
{
    int i;
    int fd;
    int ret;
    unsigned char data[READ_DATA_SIZE];
    unsigned char md5_value[MD5_SIZE];
    MD5_CTX md5;

    log_d("%s:%d open filepath : %s\n", __FUNCTION__, __LINE__, file_path);
    fd = open(file_path, O_RDONLY);
    if (-1 == fd)
    {
        //perror("open");
        log_e("%s:%d open file error!\n", __FUNCTION__, __LINE__);
        return -1;
    }

    // init md5
    MD5Init(&md5);

    while (1)
    {
        ret = read(fd, data, READ_DATA_SIZE);
        if (-1 == ret)
        {
            //perror("read");
            log_e("%s:%d read file error!\n", __FUNCTION__, __LINE__);
            close(fd);
            return -1;
        }

        MD5Update(&md5, data, ret);

        if (0 == ret || ret < READ_DATA_SIZE)
        {
            break;
        }
    }

    close(fd);

    MD5Final(&md5, md5_value);

    // convert md5 value to md5 string
    for(i = 0; i < MD5_SIZE; i++)
    {
        snprintf(md5_str + i*2, 2+1, "%02x", md5_value[i]);
    }
    log_d("%s:%d cconvert md5 result : '%s'\n", __FUNCTION__, __LINE__, md5_str);
    return 0;
}

void *check_server_thread(void *arg)
{
    //HOST_INFO check_host_info;
    //HOST_INFO download_host_info;
    //HTTP_RESPONSE_HEADER http_response_header;
    struct timespec timeout;
    is_check_update = 1;
    int ret = 0;
    
    while(is_check_update){
        ret = 0;
        int socketfd = 0;
        char url[2048] = {'\0'};
        char convert_url[2048] = {'\0'};
        char hardware[PROPERTY_VALUE_MAX] = {'\0'};
        char hardware_version[PROPERTY_VALUE_MAX] = {'\0'};
        char software_version[PROPERTY_VALUE_MAX] = {'\0'};
        char serialno[PROPERTY_VALUE_MAX] = {'\0'};
        char model[PROPERTY_VALUE_MAX] = {'\0'};
        char imei[PROPERTY_VALUE_MAX] = {'\0'};
        char check_update_result[64] = {'\0'};
        char check_update_hardware[64] = {'\0'};
        char check_update_model[64] = {'\0'};
        char check_update_file_name[64] = {'\0'};
        char check_update_file_path[64] = {'\0'};
        char check_update_file_download_url[2048] = {'\0'};
        char check_update_file_size[64] = {'\0'};
        char check_update_description[2048] = {'\0'};
        char check_update_srcVersion[1024] = {'\0'};
        char check_update_dstVersion[1024] = {'\0'};
        char check_update_file_md5[64] = {'\0'};
        
        bzero(&check_host_info,sizeof(check_host_info));
        bzero(&download_host_info,sizeof(download_host_info));
        bzero(&http_response_header,sizeof(http_response_header));
        
        property_get("ro.product.hardware", hardware, "");
        log_d("%s:%d hardware:%s\n", __FUNCTION__, __LINE__, hardware);
        
        property_get("ro.product.device", hardware_version, "");
        log_d("%s:%d hardware_version:%s\n", __FUNCTION__, __LINE__, hardware_version);
        
        property_get("ro.product.dfsl_version", software_version, "");
        log_d("%s:%d software_version:%s\n", __FUNCTION__, __LINE__, software_version);
        
        property_get("ro.boot.serialno", serialno, "");
        log_d("%s:%d serialno:%s\n", __FUNCTION__, __LINE__, serialno);
        
        property_get("ro.product.model", model, "");
        log_d("%s:%d model:%s\n", __FUNCTION__, __LINE__, model);
        
        sprintf(url, "http://121.43.183.196:8081/updsvr/ota/checkupdate?hw=%s&hwv=%s&swv=%s&serialno=%s&model=%s",
                hardware, hardware_version, software_version, serialno, model);
        
        log_d("%s:%d url:%s\n", __FUNCTION__, __LINE__, url);
        space_change(url, convert_url);
        log_d("%s:%d convert_url:%s\n", __FUNCTION__, __LINE__, convert_url);
        
        if(strlen(url) != 0){
            strcpy(check_host_info.url, convert_url);
            parse_url(check_host_info.url, check_host_info.host, &check_host_info.port, http_response_header.file_name);
            get_ip_addr(check_host_info.host, check_host_info.ip_addr);
            if (strlen(check_host_info.ip_addr) == 0){
                log_e("%s:%d parse_url failed\n", __FUNCTION__, __LINE__);
                return (void *)ERROR_PARSE_URL;
            }else{
                log_d("%s:%d parse_url success\n", __FUNCTION__, __LINE__);
                log_d("%s:%d URL: %s\n", __FUNCTION__, __LINE__, check_host_info.url);
                log_d("%s:%d Host: %s\n", __FUNCTION__, __LINE__, check_host_info.host);
                log_d("%s:%d IP Address: %s\n", __FUNCTION__, __LINE__, check_host_info.ip_addr);
                log_d("%s:%d Port: %d\n", __FUNCTION__, __LINE__, check_host_info.port);
                log_d("%s:%d FileName : %s\n", __FUNCTION__, __LINE__, http_response_header.file_name);
            }
            socketfd = socket(AF_INET, SOCK_STREAM, 0);
            if (socketfd < 0){
                log_e("%s:%d create socket failed: %d\n", __FUNCTION__, __LINE__, socketfd);
                return (void *)ERROR_CREATE_SOCKET_FAILED;
            }else{
                log_e("%s:%d create socket success: %d\n", __FUNCTION__, __LINE__, socketfd);
            }
            
            struct sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr(check_host_info.ip_addr);
            addr.sin_port = htons(check_host_info.port);
            
            log_d("%s:%d connect to host...\n", __FUNCTION__, __LINE__);
            ret = connect(socketfd, (struct sockaddr *) &addr, sizeof(addr));
            if (ret == -1){
                log_e("%s:%d create connect failed, error: %d\n", __FUNCTION__, __LINE__, ret);
                return (void *)ERROR_CONNECT_SOCKET_FAILED;
            }else{
                log_e("%s:%d create connect success, res: %d\n", __FUNCTION__, __LINE__, ret);
            }
            ret = send_http_header(socketfd, 0, &check_host_info);
            if(ret == -1){
                log_e("%s:%d send_http_header failed: %d\n", __FUNCTION__, __LINE__, ret);
                close(socketfd);
                return (void *)ERROR_SEND_HEADER_FAILED;
            }else{
                log_d("%s:%d send_http_header success: %d\n", __FUNCTION__, __LINE__, ret);
                
                ret = parse_http_response_header(socketfd, &http_response_header);
                /*log_d("http_response_header Status-Code: %d\n", http_response_header.status_code);
                log_d("http_response_header Content-Type: %s\n", http_response_header.content_type);
                log_d("http_response_header Content-Range: %s\n", http_response_header.content_range);
                log_d("http_response_header Content-Length: %ld bytes\n", http_response_header.content_length);
                log_d("http_response_header FileName: %s \n", http_response_header.file_name);
                log_d("http_response_header Transfer-Encoding : %s\n", http_response_header.transfer_encoding);*/
                
                if(http_response_header.status_code == 200){
                    log_d("%s:%d get reponse success \n\n", __FUNCTION__, __LINE__);
                    char buffer[BUFSIZ], tmp[BUFSIZ], json_buffer[BUFSIZ] = { 0 };
                    int len = 0;
                    while((len = read(socketfd, tmp, sizeof(tmp))) > 0){
                        log_d("%s:%d get reponse tmp : '%s'\n", __FUNCTION__, __LINE__, tmp);
                        if(strcmp(tmp, "0\r\n\r\n") == 0){
                            log_d("%s:%d get reponse tmp == '0\r\n' : '%s'\n", __FUNCTION__, __LINE__, tmp);
                            break;
                        }else{
                            strcpy(buffer, tmp);
                        }
                        bzero(&tmp,sizeof(tmp));
                    }
                    if(strlen(buffer) != 0){
                        encode_result(buffer, json_buffer, "{", "}");
                        log_d("%s:%d get reponse end json_buffer : \n'%s'\n", __FUNCTION__, __LINE__, json_buffer);
                        
                        char *token = strtok(json_buffer, ",");
                        while(token){
                            log_d("%s:%d get items : %s\n", __FUNCTION__, __LINE__, token);
                            char *ret;
                            if((ret = strstr(token, "CHECK_UPDATE_RESULT")) != NULL){
                                cut_str(ret, check_update_result);
                                log_d("%s:%d check_update_result %s\n", __FUNCTION__, __LINE__, check_update_result);
                            }else if((ret = strstr(token, "hardware")) != NULL){
                                cut_str(ret, check_update_hardware);
                                log_d("%s:%d check_update_hardware %s\n", __FUNCTION__, __LINE__, check_update_hardware);
                            }else if((ret = strstr(token, "model")) != NULL){
                                cut_str(ret, check_update_model);
                                log_d("%s:%d check_update_model %s\n", __FUNCTION__, __LINE__, check_update_model);
                            }else if((ret = strstr(token, "fileName")) != NULL){
                                cut_str(ret, check_update_file_name);
                                log_d("%s:%d check_update_file_name %s\n", __FUNCTION__, __LINE__, check_update_file_name);
                            }else if((ret = strstr(token, "filePath")) != NULL){
                                cut_str(ret, check_update_file_path);
                                log_d("%s:%d check_update_file_path %s\n", __FUNCTION__, __LINE__, check_update_file_path);
                            }else if((ret = strstr(token, "downloadUrl")) != NULL){
                                cut_str(ret, check_update_file_download_url);
                                log_d("%s:%d check_update_file_download_url %s\n", __FUNCTION__, __LINE__, check_update_file_download_url);
                            }else if((ret = strstr(token, "size")) != NULL){
                                cut_str(ret, check_update_file_size);
                                log_d("%s:%d check_update_file_size %s\n", __FUNCTION__, __LINE__, check_update_file_size);
                            }else if((ret = strstr(token, "description")) != NULL){
                                cut_str(ret, check_update_description);
                                log_d("%s:%d check_update_description %s\n", __FUNCTION__, __LINE__, check_update_description);
                            }else if((ret = strstr(token, "srcVersion")) != NULL){
                                cut_str(ret, check_update_srcVersion);
                                log_d("%s:%d check_update_srcVersion %s\n", __FUNCTION__, __LINE__, check_update_srcVersion);
                            }else if((ret = strstr(token, "dstVersion")) != NULL){
                                cut_str(ret, check_update_dstVersion);
                                log_d("%s:%d check_update_dstVersion %s\n", __FUNCTION__, __LINE__, check_update_dstVersion);
                            }else if((ret = strstr(token, "md5")) != NULL){
                                cut_str(ret, check_update_file_md5);
                                log_d("%s:%d check_update_file_md5 %s\n", __FUNCTION__, __LINE__, check_update_file_md5);
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
                            log_e("%s:%d check update failed check_update_file_download_url is null\n", __FUNCTION__, __LINE__);
                        }else if(strlen(check_update_file_md5) == 0){
                            log_e("%s:%d check update failed check_update_file_md5 is null\n", __FUNCTION__, __LINE__);
                        }else if(strcmp(check_update_srcVersion, software_version) != 0){
                            log_e("%s:%d cmd_server_init software_version is right target, phone:%s, server:%s\n", __FUNCTION__, __LINE__, software_version, check_update_srcVersion);
                        }else{
                            log_e("%s:%d check update failed ret %s\n", __FUNCTION__, __LINE__, check_update_result);
                        }
                        if(!access(OTA_PATCH_PATH,0)){
                            log_d("%s:%d old update file exist, delete it\n", __FUNCTION__, __LINE__, OTA_PATCH_PATH);
                            int del = remove(OTA_PATCH_PATH);
                            if(del != 0){
                                log_e("%s:%d delete bad file failed ret:%d\n", __FUNCTION__, __LINE__, del);
                            }
                        }
                    }else{
                        log_e("%s:%d get reponse items failed %d\n", __FUNCTION__, __LINE__, ERROR_INVALID_URL_REPONSE_ITEMS);
                        close(socketfd);
                        return (void *)ERROR_INVALID_URL_REPONSE_ITEMS;
                    }
                }else{
                    log_d("%s:%d get reponse status_code : %d\n", __FUNCTION__, __LINE__, http_response_header.status_code);
                    close(socketfd);
                    return (void *)ERROR_INVALID_REPONSE_STATUS_CODE;
                }
            }
            sleep(1);
            close(socketfd);
        }else{
            log_d("%s:%d create_socket ERROR_INVALID_URL\n", __FUNCTION__, __LINE__);
            return (void *)ERROR_INVALID_URL;
        }
        timeout.tv_sec = time(NULL) + THREAD_TIMEOUT;
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
            char md5_str[MD5_STR_LEN + 1];
            ret = compute_file_md5(OTA_PATCH_PATH, md5_str);
            
            if(ret == 0){
                if(strcmp(md5_str, download_host_info.md5) == 0){
                    log_d("%s:%d compare md5 success\n", __FUNCTION__, __LINE__);
                    is_download_success = 1;
                    const char *cmd_file = "/data/cache/recovery/command";
                    int fd;
                    char update_setting[100] = "--update_package=/data/update.zip\n--locale=en-US";
                    
                    ret = mkdir("/data/cache/", S_IRWXU | S_IRWXG | S_IRWXO);
                    if(-1 == ret && errno != EEXIST){
                        log_e("%s:%d mkdir /data/cache/ failed. errno is %d\n", __FUNCTION__, __LINE__, errno);
                        return (void *) -1;
                    }
                    ret = mkdir("/data/cache/recovery/", S_IRWXU | S_IRWXG | S_IRWXO);
                    if (-1 == ret && (errno != EEXIST)){
                        log_e("%s:%d mkdir /data/cache/recovery/ failed. errno is %d" , __FUNCTION__, __LINE__, errno);
                        return (void *) -1;
                    }
                    fd = open("/data/cache/recovery/command", O_WRONLY | O_CREAT, 0777);
                    if (fd >= 0){
                        ret = write(fd, update_setting, strlen(update_setting) + 1);
                        sync();
                        close(fd);
                    }else{
                        log_e("%s:%d open /data/cache/recovery/command failed", __FUNCTION__, __LINE__);
                        return (void *) -1;
                    }
                }else{
                    log_e("%s:%d compare md5 failed, download md5 is %s, server md5 is %s!", __FUNCTION__, __LINE__, md5_str, download_host_info.md5);
                    int del = remove(OTA_PATCH_PATH);
                    if(del != 0){
                        log_e("%s:%d delete bad file failed ret:%d\n", __FUNCTION__, __LINE__, del);
                    }
                    return (void *) -1;
                }
            }else{
                log_e("%s:%d compute_file_md5 failed\n", __FUNCTION__, __LINE__);
                return (void *) -1;
            }
            
            if(ret){
                sleep(1);
                log_d("%s:%d download_server_thread write cmd success\n", __FUNCTION__, __LINE__);
                android_reboot(ANDROID_RB_RESTART2, 0, "recovery");
                //execl("/system/bin/sh", "sh", "-c", "reboot recovery", (char *)0);
            }else{
                log_d("%s:%d download_server_thread write cmd failed\n", __FUNCTION__, __LINE__);
                return (void *) -2;
            }
        }else{
            log_e("%s:%d get_update_file from '%s' failed\n", __FUNCTION__, __LINE__, download_host_info.url);
        }
    }
    return (void *)ret;
}

int cmd_server_init(void)
{
    log_d("%s:%d start\n", __FUNCTION__, __LINE__);
    int ret, recv1, recv2 = 0;
    pthread_t check_td, download_td;
    
    while(1){
        ret = pthread_create(&check_td, NULL, check_server_thread, NULL);
        if(ret != 0){
            log_d("%s:%d can't create check_td %d:\n", __FUNCTION__, __LINE__, ret);
            return 0;
        }
        if(0){
            is_check_update = 0;
            pthread_cond_signal(&cond);
        }
        
        ret = pthread_join(check_td, &recv1);
        if (ret != 0){
            log_d("%s:%d cannot join with check_server_thread, recv1:%d \n", __FUNCTION__, __LINE__, recv1);
        }else{
            log_d("%s:%d join with check_server_thread ret:%d, recv1:%d\n", __FUNCTION__, __LINE__, ret, recv1);
            log_d("%s:%d check update file url:%s, md5:%s\n", __FUNCTION__, __LINE__, download_host_info.url, download_host_info.md5);
            if((int)SUCCESS_HAVE_UPDATE == recv1){
                ret = pthread_create(&download_td, NULL, download_server_thread, NULL);
                if(ret != 0){
                    log_d("%s:%d can't create download_td %d:\n", __FUNCTION__, __LINE__, ret);
                    return 0;
                }
                ret = pthread_join(download_td, &recv2);
                if(ret != 0){
                    log_d("%s:%d cannot join with download_server_thread, recv2:%d \n", __FUNCTION__, __LINE__, recv2);
                }else{
                    log_d("%s:%d join with download_server_thread ret:%d, recv2:%d\n", __FUNCTION__, __LINE__, ret, recv2);
                }
            }
        }
        sleep(60);
    }
    
    sleep(1);
    log_d("%s:%d end!\n", __FUNCTION__, __LINE__);
    return 0;
}

#ifdef __cplusplus
}
#endif

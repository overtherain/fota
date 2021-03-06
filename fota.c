#include "fota.h"
#include "get_update_file.h"
#include "check_update.h"

int main(int argc, char const *argv[])
{
    int ret = 0;
    
    if (argc == 1){
        log_e("%s:%d run cmd_server_init\n", __FUNCTION__, __LINE__);
        ret = cmd_server_init();
        return 0;
    }else if(argc == 2){
        log_d("%s:%d input url : %s\n", __FUNCTION__, __LINE__, argv[1]);
        ret = get_update_file(argv[1], "");
    }else if(argc == 3){
        log_d("%s:%d input url : %s, new name : %s\n", __FUNCTION__, __LINE__, argv[1], argv[2]);
        ret = get_update_file(argv[1], argv[2]);
    }
    log_d("%s:%d \n\n ===============get_update_file result : %d===============\n", __FUNCTION__, __LINE__, ret);
    return 0;
}

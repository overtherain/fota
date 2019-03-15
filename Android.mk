LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)
BOARD_PRODUCT_NAME:=$(TARGET_BOARD)
$(warning BOARD_PRODUCT_NAME = $(BOARD_PRODUCT_NAME))
commands_local_path := $(LOCAL_PATH)


LOCAL_C_INCLUDES += $(LOCAL_PATH)

LOCAL_STATIC_LIBRARIES += libcutils liblog
LOCAL_SHARED_LIBRARIES := libcutils

LOCAL_SRC_FILES := fota.c \
                   get_update_file.c \
                   check_update.c

LOCAL_MODULE := fota
LOCAL_MODULE_TAGS := optional
LOCAL_MULTILIB := 32

include $(BUILD_EXECUTABLE)

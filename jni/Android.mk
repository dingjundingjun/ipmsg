LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := libmsg

LOCAL_C_INCLUDES := \
    $(JNI_H_INCLUDE) \
    $(LOCAL_PATH)/h $(LOCAL_PATH)
    
LOCAL_SRC_FILES := \
    src/comm.c \
	src/file_client.c \
	src/file_server.c \
    src/lnklst.c \
    src/main.c \
    src/msg_manage.c \
    src/sys_info.c \
    src/user_manage.c \
    src/userInterface.c \
    src/utils.c \
	
    
LOCAL_CXXFLAGS := -DHAVE_PTHREADS
CFLAGS := -fpic -g 
#LOCAL_STATIC_LIBRARIES := printmath
#LOCAL_STATIC_LIBRARIES := cnocr_mips
#LOCAL_LDLIBS:=-L$(SYSROOT)/usr/lib -llog $(LOCAL_PATH)/libimage_masoic_algo.so  $(LOCAL_PATH)/libcnocr_mips.so  $(LOCAL_PATH)/libHWRecog.so $(LOCAL_PATH)/libimagepano.so 
# $(LOCAL_PATH)/libcnocr_mips.so  $(LOCAL_PATH)/libHWRecog.so
include $(BUILD_SHARED_LIBRARY)
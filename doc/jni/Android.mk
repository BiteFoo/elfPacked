LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := foo
LOCAL_SRC_FILES := foo.c
LOCAL_LDLIBS := -llog
include $(BUILD_SHARED_LIBRARY)

# include $(CLEAR_VARS)
# LOCAL_MODULE := test
# LOCAL_SRC_FILES := test.c
# LOCAL_LDLIBS := -llog
# include $(BUILD_SHARED_LIBRARY)
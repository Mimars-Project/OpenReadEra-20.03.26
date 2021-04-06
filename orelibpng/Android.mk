LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := orelibpng

LOCAL_CPPFLAGS := $(APP_CPPFLAGS)
LOCAL_CFLAGS := $(APP_CFLAGS)
LOCAL_CFLAGS += -DHAVE_CONFIG_H

LOCAL_SRC_FILES += \
    pngerror.c  \
    pngget.c  \
    pngpread.c \
    pngrio.c \
    pngrutil.c \
    pngvcrd.c \
    png.c \
    pngwrite.c \
    pngwutil.c \
    pnggccrd.c \
    pngmem.c \
    pngread.c \
    pngrtran.c \
    pngset.c \
    pngtrans.c \
    pngwio.c \
    pngwtran.c

include $(BUILD_STATIC_LIBRARY)
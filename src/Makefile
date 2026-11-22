all : flash

TARGET := ebook
TARGET_MCU := CH32V203
TARGET_MCU_PACKAGE := C8T6
ADDITIONAL_C_FILES := peripherals.h peripherals.c ports.h eink.h eink.c renderer.h renderer.c page.h page.c

include ../ch32fun/ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean


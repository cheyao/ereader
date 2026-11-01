all : flash

TARGET := ebook
TARGET_MCU := CH32V203
ADDITIONAL_C_FILES := peripherals.h peripherals.c ports.h eink.h eink.c

include ../ch32fun/ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean


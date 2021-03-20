SYS_NAME=Linux
CPU_NAME=X64
mkdir=if [ ! -d "$1" ]; then mkdir "$1" fi

ifeq ($(OS),Windows_NT)
	SYS_NAME=WIN32
	mkdir=IF exist "$1" () ELSE (mkdir "$1")
    ifeq ($(PROCESSOR_ARCHITEW6432),AMD64)
		CPU_NAME=X86_64
    else
        ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
			CPU_NAME=X64
		else
			CPU_NAME=X86
        endif
    endif
else
    UNAME_S:=$(shell uname -s)
    UNAME_P:=$(shell uname -p)
    SYS_NAME:=$(UNAME_S)
    ifeq ($(UNAME_P),x86_64)
        CPU_NAME=X86_64
    endif
    ifneq ($(filter %86,$(UNAME_P)),)
        CPU_NAME=X86
    endif
    ifneq ($(filter arm%,$(UNAME_P)),)
		CPU_NAME=ARM
    endif
endif

TARGET_SYS_NAME?=$(SYS_NAME)
TARGET_CPU_NAME?=$(CPU_NAME)

include mak/sys/$(TARGET_SYS_NAME).mak
include mak/cpu/$(TARGET_CPU_NAME).mak

SYS_EXE_SFX:=$(SYS_$(TARGET_SYS_NAME)_EXE_SFX)
SYS_DLL_PFX:=$(SYS_$(TARGET_SYS_NAME)_DLL_PFX)
SYS_DLL_SFX:=$(SYS_$(TARGET_SYS_NAME)_DLL_SFX)
SYS_DEFINES:=$(SYS_$(TARGET_SYS_NAME)_DEFINES)
CPU_DEFINES:=$(CPU_$(TARGET_CPU_NAME)_DEFINES)

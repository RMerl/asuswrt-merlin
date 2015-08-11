export CC = $(CROSS_COMPILE)gcc -c
export AR = $(CROSS_COMPILE)ar rv 
export LD = $(CROSS_COMPILE)gcc
export LDOUT = -o 
export RANLIB = $(CROSS_COMPILE)ranlib

export OBJEXT := .o
export LIBEXT := .a
export LIBEXT2 :=

export CC_OUT := -o 
export CC_INC := -I
export CC_DEF := -D
export CC_OPTIMIZE := -O2
export CC_LIB := -l

export CC_SOURCES :=
export CC_CFLAGS := -Wall 
#export CC_CFLAGS += -Wdeclaration-after-statement
#export CC_CXXFLAGS := -Wdeclaration-after-statement
export CC_LDFLAGS :=


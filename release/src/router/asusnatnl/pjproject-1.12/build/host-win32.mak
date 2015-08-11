export HOST_MV := ren
export HOST_RM := if exist @@; del /F /Q @@
export HOST_RMR := if exist @@; del /F /Q @@
export HOST_RMDIR := if exist @@; rmdir @@
export HOST_MKDIR := if not exist @@; mkdir @@
export HOST_EXE := .exe
export HOST_PSEP := \\

export HOST_SOURCES :=
export HOST_CFLAGS :=
export HOST_CXXFLAGS :=
export HOST_LDFLAGS :=

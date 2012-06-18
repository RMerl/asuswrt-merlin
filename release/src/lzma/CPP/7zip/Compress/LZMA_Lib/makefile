PROG = liblzma.a
AR = ar
RM = rm -f
CFLAGS = -O3 -Wall

OBJS = \
  ZLib.o \
  Alloc.o \
  7zCrc.o \
  MatchFinder.o \
  LZMADecoder.o \
  LZMAEncoder.o \
  LZOutWindow.o \
  RangeCoderBit.o \
  InBuffer.o \
  OutBuffer.o \
  FileStreams.o \
  StreamUtils.o \
  C_FileIO.o \
  CommandLineParser.o \
  CRC.o \
  NewHandler.o \
  MyString.o \
  StringConvert.o \
  StringToInt.o \
  MyVector.o \
  UTFConvert.o \


all: $(PROG)

$(PROG): $(OBJS)
	$(AR) r $(PROG) $(OBJS)

vpath %.cpp ../LZMA/ ../LZ/ ../../Common/ ../../../Common/ ../RangeCoder/
vpath %.c   ../../../../C/ ../../../../C/Compress/Lz/

%.o: %.cpp
	$(CXX) -c $(CFLAGS) $<

%.o: %.c
	$(CC) -c $(CFLAGS) $<

ZLib.o: ZLib.cpp


clean:
	-$(RM) $(PROG) $(OBJS)


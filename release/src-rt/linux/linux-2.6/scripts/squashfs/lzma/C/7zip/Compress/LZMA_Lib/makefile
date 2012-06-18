PROG = liblzma.a
CXX = g++ -O3 -Wall
AR = ar
RM = rm -f
CFLAGS = -c  -I ../../../

OBJS = \
  ZLib.o \
  LZMADecoder.o \
  LZMAEncoder.o \
  LZInWindow.o \
  LZOutWindow.o \
  RangeCoderBit.o \
  InBuffer.o \
  OutBuffer.o \
  FileStreams.o \
  Alloc.o \
  C_FileIO.o \
  CommandLineParser.o \
  CRC.o \
  StreamUtils.o \
  String.o \
  StringConvert.o \
  StringToInt.o \
  Vector.o \


all: $(PROG)

$(PROG): $(OBJS)
	$(AR) r $(PROG) $(OBJS)

ZLib.o: ZLib.cpp
	$(CXX) $(CFLAGS) ZLib.cpp

LZMADecoder.o: ../LZMA/LZMADecoder.cpp
	$(CXX) $(CFLAGS) ../LZMA/LZMADecoder.cpp

LZMAEncoder.o: ../LZMA/LZMAEncoder.cpp
	$(CXX) $(CFLAGS) ../LZMA/LZMAEncoder.cpp

LZInWindow.o: ../LZ/LZInWindow.cpp
	$(CXX) $(CFLAGS) ../LZ/LZInWindow.cpp

LZOutWindow.o: ../LZ/LZOutWindow.cpp
	$(CXX) $(CFLAGS) ../LZ/LZOutWindow.cpp

RangeCoderBit.o: ../RangeCoder/RangeCoderBit.cpp
	$(CXX) $(CFLAGS) ../RangeCoder/RangeCoderBit.cpp

InBuffer.o: ../../Common/InBuffer.cpp
	$(CXX) $(CFLAGS) ../../Common/InBuffer.cpp

OutBuffer.o: ../../Common/OutBuffer.cpp
	$(CXX) $(CFLAGS) ../../Common/OutBuffer.cpp

StreamUtils.o: ../../Common/StreamUtils.cpp
	$(CXX) $(CFLAGS) ../../Common/StreamUtils.cpp

FileStreams.o: ../../Common/FileStreams.cpp
	$(CXX) $(CFLAGS) ../../Common/FileStreams.cpp

Alloc.o: ../../../Common/Alloc.cpp
	$(CXX) $(CFLAGS) ../../../Common/Alloc.cpp

C_FileIO.o: ../../../Common/C_FileIO.cpp
	$(CXX) $(CFLAGS) ../../../Common/C_FileIO.cpp

CommandLineParser.o: ../../../Common/CommandLineParser.cpp
	$(CXX) $(CFLAGS) ../../../Common/CommandLineParser.cpp

CRC.o: ../../../Common/CRC.cpp
	$(CXX) $(CFLAGS) ../../../Common/CRC.cpp

MyWindows.o: ../../../Common/MyWindows.cpp
	$(CXX) $(CFLAGS) ../../../Common/MyWindows.cpp

String.o: ../../../Common/String.cpp
	$(CXX) $(CFLAGS) ../../../Common/String.cpp

StringConvert.o: ../../../Common/StringConvert.cpp
	$(CXX) $(CFLAGS) ../../../Common/StringConvert.cpp

StringToInt.o: ../../../Common/StringToInt.cpp
	$(CXX) $(CFLAGS) ../../../Common/StringToInt.cpp

Vector.o: ../../../Common/Vector.cpp
	$(CXX) $(CFLAGS) ../../../Common/Vector.cpp

clean:
	-$(RM) $(PROG) $(OBJS)


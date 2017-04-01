CXXFLAGS=-Werror -std=c++11

SRCS=i2psam.cpp
OBJS=$(SRCS:.cpp=.o)
TARGET=libi2psam.a

$(TARGET): $(OBJS)
	$(AR) $(ARFLAGS) $(TARGET) $(OBJS)

clean:
	$(RM) $(TARGET) $(OBJS)

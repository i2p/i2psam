CXXFLAGS=-Werror -std=c++11 -g

SRCS=i2psam.cpp
OBJS=$(SRCS:.cpp=.o)
TARGET=libi2psam.a

$(TARGET): $(OBJS)
	$(AR) $(ARFLAGS) $(TARGET) $(OBJS)

LOADLIBES=-L./ -li2psam

eepget: eepget.cpp $(TARGET)

clean:
	$(RM) $(TARGET) $(OBJS) eepget

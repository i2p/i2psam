CXXFLAGS=-Werror -Wall -Wextra -Wno-unused-parameter -std=c++11 -g

SRCS=i2psam.cpp
OBJS=$(SRCS:.cpp=.o)
TARGET=libi2psam.a

$(TARGET): $(OBJS)
	$(AR) $(ARFLAGS) $(TARGET) $(OBJS)

LOADLIBES=-L./ -li2psam -lboost_system  -lboost_thread -lpthread

eepget: eepget.cpp $(TARGET)

export USE_BOOST=1

boost: boost.cpp $(TARGET)

clean:
	$(RM) $(TARGET) $(OBJS) eepget

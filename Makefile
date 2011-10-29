CXX      = g++
CPPFLAGS = -DLINUX 
CXXFLAGS = -m32 -fPIC -Wno-attributes 
LDFLAGS  = -m32 -shared -lsampgdk -lboost_system -lboost_date_time -lboost_regex -lboost_random ~/websocketpp/libwebsocketpp.a

all: sampmap.so

sampmap.so: sampmap.o 
	$(CXX) $^ -o $@ $(LDFLAGS)

clean:
	rm -vf sampmap.o sampmap.so
CXX = g++
CXXFLAGS = -std=c++11 -Wall

all: server send_data

server: server.cpp
	$(CXX) $(CXXFLAGS) -o server server.cpp

send_data: send_data.cpp
	$(CXX) $(CXXFLAGS) -o send_data send_data.cpp

clean:
	rm -f server send_data *.txt

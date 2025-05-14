# Makefile tema 2
CXX=g++
CXXFLAGS=-Wall -g

all: server subscriber

server: server.cpp utils.h
	$(CXX) $(CXXFLAGS) -o server server.cpp

subscriber: subscriber.cpp utils.h
	$(CXX) $(CXXFLAGS) -o subscriber subscriber.cpp

clean:
	rm -f server subscriber

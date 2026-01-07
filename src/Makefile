CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra

TARGET = golf_app
OBJS = main.o ball_vector.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(OBJS) -O $(TARGET)

main.o: main.cpp ball_vector.h
	$(CXX) $(CXXFLAGS) -c main.cpp

ball_vector.o: ball_vector.cpp ball_vector.h
	$(CXX) $(CXXFLAGS) -c ball_vector.cpp

clean:
	rm -f $(OBJS) $(TARGET)

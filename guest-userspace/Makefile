CXX = clang++
CXXFLAGS = -std=c++11 -Wall -Wextra -I../protocol/include
LDFLAGS = -framework CoreFoundation -framework IOKit

TARGET = GravityGPU_TestApp
SRCS = main.cpp

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRCS) $(LDFLAGS)

clean:
	rm -f $(TARGET)

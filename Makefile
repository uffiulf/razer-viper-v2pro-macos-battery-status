CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
OBJCFLAGS = -x objective-c++ -std=c++17 -Wall -Wextra -O2

# IOKit-based implementation - no HIDAPI needed
FRAMEWORKS = -framework IOKit -framework Cocoa -framework CoreFoundation

SRCDIR = src
SOURCES = $(SRCDIR)/RazerDevice.cpp $(SRCDIR)/main.mm
OBJECTS = $(SOURCES:.cpp=.o)
OBJECTS := $(OBJECTS:.mm=.o)

TARGET = RazerBatteryMonitor

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(FRAMEWORKS)

$(SRCDIR)/RazerDevice.o: $(SRCDIR)/RazerDevice.cpp $(SRCDIR)/RazerDevice.hpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(SRCDIR)/main.o: $(SRCDIR)/main.mm $(SRCDIR)/RazerDevice.hpp
	$(CXX) $(OBJCFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: all clean

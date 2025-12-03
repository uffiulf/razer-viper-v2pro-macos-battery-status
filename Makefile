CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2
OBJCFLAGS = -x objective-c++ -std=c++17 -Wall -Wextra -O2

# Auto-detect hidapi via pkg-config
HIDAPI_CFLAGS = $(shell pkg-config --cflags hidapi 2>/dev/null || echo "-I/opt/homebrew/include/hidapi")
HIDAPI_LIBS = $(shell pkg-config --libs hidapi 2>/dev/null || echo "-L/opt/homebrew/lib -lhidapi")

FRAMEWORKS = -framework IOKit -framework Cocoa -framework CoreFoundation

SRCDIR = src
SOURCES = $(SRCDIR)/RazerDevice.cpp $(SRCDIR)/main.mm
OBJECTS = $(SOURCES:.cpp=.o)
OBJECTS := $(OBJECTS:.mm=.o)

TARGET = RazerBatteryMonitor
SCANNER = CommandScanner
ANALYZER = InterfaceAnalyzer
QUICKTEST = QuickTest

all: $(TARGET)

scanner: $(SCANNER)

analyzer: $(ANALYZER)

quicktest: $(QUICKTEST)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(HIDAPI_LIBS) $(FRAMEWORKS)

$(SRCDIR)/RazerDevice.o: $(SRCDIR)/RazerDevice.cpp $(SRCDIR)/RazerDevice.hpp
	$(CXX) $(CXXFLAGS) $(HIDAPI_CFLAGS) -c $< -o $@

$(SRCDIR)/main.o: $(SRCDIR)/main.mm $(SRCDIR)/RazerDevice.hpp
	$(CXX) $(OBJCFLAGS) $(HIDAPI_CFLAGS) -c $< -o $@

$(SCANNER): $(SRCDIR)/CommandScanner.cpp
	$(CXX) $(CXXFLAGS) $(HIDAPI_CFLAGS) $< -o $@ $(HIDAPI_LIBS) $(FRAMEWORKS)

$(ANALYZER): $(SRCDIR)/InterfaceAnalyzer.cpp
	$(CXX) $(CXXFLAGS) $(HIDAPI_CFLAGS) $< -o $@ $(HIDAPI_LIBS) $(FRAMEWORKS)

$(QUICKTEST): $(SRCDIR)/QuickTest.cpp
	$(CXX) $(CXXFLAGS) $(HIDAPI_CFLAGS) $< -o $@ $(HIDAPI_LIBS) $(FRAMEWORKS)

clean:
	rm -f $(OBJECTS) $(TARGET) $(SCANNER) $(ANALYZER) $(QUICKTEST)

.PHONY: all clean scanner analyzer quicktest


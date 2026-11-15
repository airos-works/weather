.DEFAULT_GOAL := all
CXX ?= g++
BUILD ?= build
PRIVATE_HEADERS ?= /boot/system/develop/headers/private
CPPFLAGS += -Isrc -I$(PRIVATE_HEADERS)/netservices
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Wextra -Wno-multichar -MMD -MP
SOURCES = src/App.cpp src/Weather.cpp src/Json.cpp src/Network.cpp src/Settings.cpp src/Render.cpp
OBJECTS = $(SOURCES:src/%.cpp=$(BUILD)/%.o)
LDLIBS = -lbe -lnetservices -lshared -lbnetapi -lnetwork -ldevice -ltranslation
RC ?= rc
XRES ?= xres
MIMESET ?= mimeset
all: $(BUILD)/Weather
$(BUILD)/%.o: src/%.cpp
	mkdir -p $(BUILD)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c $< -o $@
$(BUILD)/Weather: $(OBJECTS) resources/Weather.rdef resources/Weather-icon32.raw resources/Weather-icon16.raw
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@.new
	$(RC) -o $(BUILD)/Weather.rsrc resources/Weather.rdef
	$(XRES) -o $@.new $(BUILD)/Weather.rsrc
	$(MIMESET) -f $@.new
	mv $@.new $@
	cp -R resources $(BUILD)/
check-host:
	mkdir -p build-host
	g++ -std=c++17 -Wall -Wextra -g -O1 -fsanitize=address,undefined -Isrc tests/core_test.cpp src/Weather.cpp src/Json.cpp -o build-host/core_test
	build-host/core_test
install: all
	bash tools/install.sh $(BUILD)
$(BUILD)/native_test: tests/native_test.cpp $(filter-out $(BUILD)/App.o,$(OBJECTS))
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ $(LDFLAGS) $(LDLIBS) -o $@
$(BUILD)/layout_test: tests/layout_test.cpp $(BUILD)/Render.o $(BUILD)/Weather.o $(BUILD)/Json.o
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $^ $(LDFLAGS) -lbe -ltranslation -o $@
check-native: $(BUILD)/native_test
	$(BUILD)/native_test
$(BUILD)/workflow_test: tests/workflow_test.cpp
	$(CXX) $(CXXFLAGS) $< -lbe -o $@
clean:
	rm -rf $(BUILD)
-include $(OBJECTS:.o=.d)
.PHONY: all check-host install clean

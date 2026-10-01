PROJECT_ROOT = $(dir $(abspath $(lastword $(MAKEFILE_LIST))))

CC = clang
CXX = clang++
OBJS = sensor_tx.o time_monitor.o

ifeq ($(BUILD_MODE),debug)
	CFLAGS += -g -O0
else ifeq ($(BUILD_MODE),run)
	CFLAGS += -O2
else ifeq ($(BUILD_MODE),profile)
	CFLAGS += -g -pg -fprofile-arcs -ftest-coverage
	LDFLAGS += -pg -fprofile-arcs -ftest-coverage
	EXTRA_CLEAN += sensor_tx.gcda sensor_tx.gcno $(PROJECT_ROOT)gmon.out
	EXTRA_CMDS = rm -rf sensor_tx.gcda
else
    $(error Build mode $(BUILD_MODE) not supported by this Makefile)
endif

all:	sensor_tx

sensor_tx:	$(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^
	$(EXTRA_CMDS)

%.o:	$(PROJECT_ROOT)%.cpp
	$(CXX) -c $(CFLAGS) $(CXXFLAGS) $(CPPFLAGS) -o $@ $<

%.o:	$(PROJECT_ROOT)%.c
	$(CC) -c $(CFLAGS) $(CPPFLAGS) -o $@ $<

clean:
	-del /F /Q sensor_tx.exe sensor_tx.o time_monitor.o $(EXTRA_CLEAN) 2>nul

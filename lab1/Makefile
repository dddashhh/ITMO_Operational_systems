# Compiler
CXX = g++
CXXFLAGS = -O0 -std=c++17
PTHREAD = -pthread

# Source files
SRC_EMA_SORT_INT = ema-sort-int.cpp
SRC_DEDUP = dedup.cpp
SRC_THREADED_LOAD = threaded_load.cpp
SRC_IO_LAT_WRITE = io-lat-write.cpp
SRC_SHELL = shell.cpp

# Test source files
SRC_TEST_SHELL = tests/test_shell.cpp
SRC_TEST_EMA_SORT_INT = tests/test_ema_sort_int.cpp
SRC_TEST_DEDUP = tests/test_dedup.cpp

# Executables
EXE_EMA_SORT_INT = ema-sort-int
EXE_DEDUP = dedup
EXE_THREADED_LOAD = threaded_load
EXE_IO_LAT_WRITE = io-lat-write
EXE_SHELL = shell

# Test Executables
EXE_TEST_SHELL = test_shell
EXE_TEST_EMA_SORT_INT = test_ema_sort_int
EXE_TEST_DEDUP = test_dedup

# All executables
ALL_EXES = $(EXE_EMA_SORT_INT) $(EXE_DEDUP) $(EXE_THREADED_LOAD) $(EXE_SHELL)

# All test executables
ALL_TEST_EXES = $(EXE_TEST_SHELL) $(EXE_TEST_EMA_SORT_INT) $(EXE_TEST_DEDUP)

all: $(ALL_EXES) $(ALL_TEST_EXES)

$(EXE_EMA_SORT_INT): $(SRC_EMA_SORT_INT)
	$(CXX) -o $@ $< $(CXXFLAGS)

$(EXE_DEDUP): $(SRC_DEDUP)
	$(CXX) -o $@ $< $(CXXFLAGS)

$(EXE_THREADED_LOAD): $(SRC_THREADED_LOAD)
	$(CXX) -o $@ $< $(CXXFLAGS) $(PTHREAD)

$(EXE_SHELL): $(SRC_SHELL)
	$(CXX) -o $@ $< $(CXXFLAGS)

$(EXE_TEST_SHELL): $(SRC_TEST_SHELL)
	$(CXX) -o $@ $< $(CXXFLAGS)

$(EXE_TEST_EMA_SORT_INT): $(SRC_TEST_EMA_SORT_INT)
	$(CXX) -o $@ $< $(CXXFLAGS)

$(EXE_TEST_DEDUP): $(SRC_TEST_DEDUP)
	$(CXX) -o $@ $< $(CXXFLAGS)

run: $(EXE_SHELL)
	./$(EXE_SHELL)

test: $(ALL_TEST_EXES)
	./$(EXE_TEST_SHELL)
	./$(EXE_TEST_EMA_SORT_INT)
	./$(EXE_TEST_DEDUP)

clean:
	rm -f $(ALL_EXES) $(ALL_TEST_EXES) *.bin *.txt *.tmp merged_* temp_*

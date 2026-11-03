CXX       ?= clang++
CXXFLAGS  := -std=c++17 -Wall -Wextra -Werror
INCLUDES  := -I include
BUILDDIR  := build

# ─── Build targets ───────────────────────────────────────────────
.PHONY: all demos tests benchmarks clean

all: demos tests

demos: $(BUILDDIR)/shortest_paths_demo $(BUILDDIR)/signed_core_demo $(BUILDDIR)/branch_core_demo

tests: $(BUILDDIR)/shortest_paths_test $(BUILDDIR)/signed_core_test

benchmarks: $(BUILDDIR)/shortest_paths_benchmark $(BUILDDIR)/signed_core_benchmark

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# ─── Demos ───────────────────────────────────────────────────────
$(BUILDDIR)/shortest_paths_demo: src/shortest_paths_demo.cpp include/shortest_paths.hpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -O2 $(INCLUDES) $< -o $@

$(BUILDDIR)/signed_core_demo: src/signed_core_demo.cpp include/signed_core.hpp include/shortest_paths.hpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -O2 $(INCLUDES) $< -o $@

$(BUILDDIR)/branch_core_demo: src/branch_core_demo.cpp include/branch_core.hpp include/signed_core.hpp include/shortest_paths.hpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -O2 $(INCLUDES) $< -o $@

# ─── Tests (with sanitizers) ────────────────────────────────────
$(BUILDDIR)/shortest_paths_test: tests/shortest_paths_test.cpp include/shortest_paths.hpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -O1 -g -fsanitize=address,undefined $(INCLUDES) $< -o $@

$(BUILDDIR)/signed_core_test: tests/signed_core_test.cpp include/branch_core.hpp include/signed_core.hpp include/shortest_paths.hpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -O1 -g -fsanitize=address,undefined $(INCLUDES) $< -o $@

# ─── Benchmarks ─────────────────────────────────────────────────
$(BUILDDIR)/shortest_paths_benchmark: benchmarks/shortest_paths_benchmark.cpp include/shortest_paths.hpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -O2 $(INCLUDES) $< -o $@

$(BUILDDIR)/signed_core_benchmark: benchmarks/signed_core_benchmark.cpp include/branch_core.hpp include/signed_core.hpp include/shortest_paths.hpp | $(BUILDDIR)
	$(CXX) $(CXXFLAGS) -O2 $(INCLUDES) $< -o $@

# ─── Run targets ────────────────────────────────────────────────
.PHONY: run-tests run-benchmarks

run-tests: tests
	@echo "=== Shortest Paths Tests ==="
	$(BUILDDIR)/shortest_paths_test
	@echo ""
	@echo "=== Signed Core + Branch Core Tests ==="
	$(BUILDDIR)/signed_core_test

run-benchmarks: benchmarks
	@echo "=== Shortest Paths Benchmark ==="
	$(BUILDDIR)/shortest_paths_benchmark
	@echo ""
	@echo "=== Signed Core Benchmark ==="
	$(BUILDDIR)/signed_core_benchmark

# ─── Clean ──────────────────────────────────────────────────────
clean:
	rm -rf $(BUILDDIR)

#
# Build the self tests
#

CXXFLAGS += --std=c++11 -Wall

SRCS := $(wildcard *.cc)
OBJS := $(SRCS:%.cc=%.o)
DEPS := $(OBJS:.o=.d)

%.o: %.cc
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

selftest: $(OBJS)
	$(CXX) $< -o $@

test: selftest
	./selftest

clean:
	rm -f selftest *.o *.d

.PHONY: all test clean

-include $(DEPS)

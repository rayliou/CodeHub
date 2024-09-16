CXXFLAGS=-std=c++20 -g
LDFLAGS=-g -lev

example_coroutine:example_coroutine.cpp logger.cpp
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^
clean:
	rm -vf example_coroutine
test:
	./example_coroutine

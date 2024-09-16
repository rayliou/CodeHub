CXXFLAGS=-std=c++20 -g
LDFLAGS=-g -lev

example_coroutine:example_coroutine.cpp logger.cpp
	$(CXX) $(CXXFLAGS)  -o $@ $^ $(LDFLAGS)
clean:
	rm -vf example_coroutine
test:
	./example_coroutine
# Ubuntu 20.04
# apt install -y g++-10  make git libev-dev vim
# g++-10  -std=c++20 -g -fcoroutines  -o example_coroutine example_coroutine.cpp logger.cpp -g -lev


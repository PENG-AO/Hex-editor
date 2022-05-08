CPPFLAGS = -std=c++14 -stdlib=libc++

srcs = $(wildcard *.cpp)
objs = $(patsubst %.cpp, %.o, $(srcs))

all: editor

editor: $(objs)
	g++ -o $@ $^ -lncurses

.PHONY: clean
clean:
	rm editor $(objs)

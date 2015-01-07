CC:=clang++
CFLAGS:=-std=c++11 -g -Wall $(shell sdl2-config --cflags) -DHANDMADE_INTERNAL=1
LIB:=-std=c++11 $(shell sdl2-config --libs)
DEPS:=$(wildcard src/*.hpp)
SRC:=$(wildcard src/*.cpp)
OBJ:=$(patsubst src/%.cpp,%.o,$(SRC))


all: HandmadeHero

%.o: src/%.cpp $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $< 

HandmadeHero: $(OBJ)
	$(CC) $(LIB) -o $@ $^ 

clean:
	rm -f *.o HandmadeHero

CC:=clang++
PLATFORM_CFLAGS:=-std=c++11 -g -Wall $(shell sdl2-config --cflags) -DHANDMADE_INTERNAL=1 
GAME_CFLAGS:=-fPIC -std=c++11 -g -Wall -DHANDMADE_INTERNAL=1 
PLATFORM_LIB:=-std=c++11 $(shell sdl2-config --libs) -ldl
GAME_LIB:= -std=c++11  
PLATFORM_DEPS:= ./src/handmade.hpp ./src/sdl_main.hpp
PLATFORM_SRC:= ./src/sdl_main.cpp
GAME_DEPS:= ./src/handmade.hpp
GAME_SRC:= ./src/handmade.cpp
PLATFORM_OBJ:=$(patsubst ./src/%.cpp,%.o,$(PLATFORM_SRC))
GAME_OBJ:=$(patsubst ./src/%.cpp,%.o,$(GAME_SRC))


all: HandmadeHero GameLib

#%.o: src/%.cpp $(PLATFORM_DEPS) $(GAME_DEPS)
#	$(CC) $(CFLAGS) -c -o $@ $< 

$(PLATFORM_OBJ): $(PLATFORM_SRC) $(PLATFORM_DEPS)
	$(CC) $(PLATFORM_CFLAGS) -c -o $@ $< 


$(GAME_OBJ): $(GAME_SRC) $(GAME_DEPS)
	$(CC) $(GAME_CFLAGS)  -c -o $@ $< 

HandmadeHero: $(PLATFORM_OBJ)
	$(CC) $(PLATFORM_LIB) -o $@ $^

GameLib: $(GAME_OBJ)
	$(CC) $(GAME_LIB) -o game-tmp.so -fPIC -shared $^
	mv game-tmp.so game.so

clean:
	rm -f *.o HandmadeHero

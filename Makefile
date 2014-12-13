CC:=clang++
CFLAGS:=-std=c++11  $(shell sdl2-config --cflags)
LIB:=-std=c++11 $(shell sdl2-config --libs)
DEPS:=src/handmade.h
OBJ:=src/sdl_main.o src/handmade.o 

all: HandmadeHero

.cpp.o: $(DEPS)
	$(CC) $(CFLAGS) -c -o $@ $< 
HandmadeHero: $(OBJ)
	$(CC) $(LIB) -o $@ $^ 

clean:
	rm -f *.o HandmadeHero

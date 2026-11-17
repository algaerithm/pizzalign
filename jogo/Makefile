# Pizzalign - compilar com:  make
# Se a raylib estiver em outra pasta:  make RAYLIB=C:/pizzalign/raylib/src
RAYLIB ?= raylib/src
CC     ?= gcc
CFLAGS ?= -O2 -Wall -Wextra -std=c11

ifeq ($(OS),Windows_NT)
  EXE  = pizzalign.exe
  LIBS = -lraylib -lopengl32 -lgdi32 -lwinmm -lm
else
  EXE  = pizzalign
  LIBS = -lraylib -lGL -lm -lpthread -ldl -lrt -lX11
endif

SRCS = main.c motor.c
HDRS = motor.h assets_embutidos.h

all: $(EXE)

$(EXE): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) $(SRCS) -o $(EXE) -I $(RAYLIB) -L $(RAYLIB) $(LIBS)

debug: CFLAGS = -g -O0 -Wall -Wextra -std=c11
debug: clean $(EXE)

# so se voce trocar algo em assets/ (precisa de Python)
embutidos:
	python gerar_embutidos.py

clean:
	-rm -f pizzalign pizzalign.exe

.PHONY: all debug embutidos clean
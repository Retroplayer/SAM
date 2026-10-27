CC ?= cc
CFLAGS ?= -O2 -g
WARN = -Wall -Wextra -Werror -Wpedantic
REF = .deps/arduino-SAM/src
CPPFLAGS += -Iinclude -Icompat -I$(REF)
REFERENCE = $(REF)/sam.c $(REF)/render.c $(REF)/reciter.c $(REF)/sam_debug.c

.PHONY: all reference test sanitize demo clean
all: build/robot-sam
reference:
	sh tools/fetch-reference.sh
build:
	mkdir -p build
build/robot.o: src/robot.c include/robot.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -std=c11 -c $< -o $@
build/render.o: src/render.c include/robot.h $(REF)/sam.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -std=c11 -c $< -o $@
build/main.o: src/main.c include/robot.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -std=c11 -c $< -o $@
build/profile.o: src/profile.c include/robot.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -std=c11 -c $< -o $@
build/reference-%.o: $(REF)/%.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) -std=gnu11 -fno-strict-aliasing -c $< -o $@
REF_OBJECTS = $(addprefix build/reference-,sam.o render.o reciter.o sam_debug.o)
build/robot-sam: build/robot.o build/render.o build/main.o build/profile.o $(REF_OBJECTS)
	$(CC) $(CFLAGS) $^ -o $@
build/test-robot: tests/test_robot.c build/robot.o | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -std=c11 $^ -o $@
test: build/test-robot build/robot-sam
	./build/test-robot
	python3 tests/test_audio.py
sanitize: | build
	$(CC) $(CPPFLAGS) -O1 -g -std=c11 $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_robot.c src/robot.c -o build/test-sanitize
	./build/test-sanitize
demo: build/robot-sam
	./build/robot-sam --character COMPANION --wav build/companion.wav 'Hello, my friend. Are you ready? The CPU is online. You have been warned!'
	./build/robot-sam --character DICTATOR --wav build/dictator.wav 'Hello, my friend. Are you ready? The CPU is online. You have been warned!'
	./build/robot-sam --character MALFUNCTION --wav build/malfunction.wav 'Hello, my friend. Are you ready? The CPU is online. You have been warned!'
clean:
	rm -rf build

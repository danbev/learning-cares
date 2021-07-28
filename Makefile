CARES_DIR = ../c-ares
objs := $(patsubst src/%.c, out/%,  $(wildcard src/*.c))

all: ${objs}

out/%: src/%.c | out
	${CC} ${CFLAGS} -g -I${CARES_DIR}/include  -I${CARES_DIR}/build -I./src -o $@ -L${CARES_DIR}/build/lib64/ -lcares $<

out:
	@mkdir out


.PHONY: clean
clean: 
	@${RM} -rf out

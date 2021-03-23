PWD=$(shell pwd)
CC=gcc
CSTD=-std=c99
CFLAGS_REL=-Os -s -mtune=generic
CFLAGS_DBG=-g -O0 -D DEBUG -mtune=generic -fsanitize=address,leak
DOCKER_CONT_NAME=buildenv-qemu-run

all: release
.PHONY: clean_rm_config_h clean_rm_all_binaries genhashes release_binary debug_binary clean release debug docker

clean_rm_config_h:
	rm -rv config.h || test 1

clean_rm_all_binaries:
	rm -rv *.bin || test 1

genhashes:
	${CC} ${CFLAGS_DBG} -w genhashes.c -o genhashes.bin
	chmod -v +x genhashes.bin
	./genhashes.bin

release_binary:
	${CC} ${CSTD} ${CFLAGS_REL} qemu-run.c -o qemu-run.bin

debug_binary:
	${CC} ${CSTD} ${CFLAGS_DBG} qemu-run.c -o qemu-run.bin

clean: clean_rm_config_h clean_rm_all_binaries
release: clean genhashes release_binary clean
debug: clean genhashes debug_binary clean

docker:
	docker rmi ${DOCKER_CONT_NAME}:latest || test 1
	docker build --force-rm --no-cache -t ${DOCKER_CONT_NAME} .
	docker run -it -v ${PWD}:/app -w /app ${DOCKER_CONT_NAME} make
	docker rmi -f ${DOCKER_CONT_NAME}:latest

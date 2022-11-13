CFLAGS = -g3 -Wall -Wextra -Wconversion -Wcast-qual -Wcast-align -g
CFLAGS += -Winline -Wfloat-equal -Wnested-externs
CFLAGS += -pedantic -std=gnu99 -Werror

PROMPT = -DPROMPT

EXECS = 33sh, 33noprompt

.PHONY: clean

33sh: jobs.c sh.c
	#TODO: compile your program, including the -DPROMPT macro
	$(CC) $(CFLAGS) -DPROMPT $^ -o $@
33noprompt: jobs.c sh.c
	#TODO: compile your program without the prompt macro
	$(CC) $(CFLAGS) $^ -o $@
all: 33sh 33noprompt
clean:
	#TODO: clean up any executable files that this Makefile has produced
	rm -f $(EXECS)
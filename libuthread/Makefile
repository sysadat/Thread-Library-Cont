# Target library
lib        := libuthread.a
objs       := sem.o tps.o
given_objs := queue.o thread.o

CC     := gcc
STATIC := ar
STATIC += rcs

CFLAGS := -Wall -Werror -Wextra

ifneq ($(V),1)
Q = @
endif

all: $(lib)

deps := $(patsubst %.o, %.d, $(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

%.a: $(objs)
	@echo "CC $@"
	$(Q) $(STATIC) $@ $^ $(given_objs)

%.o: %.c
	@echo "CC $@"
	$(Q) $(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean:
	@echo "clean"
	$(Q) rm -f $(lib) $(objs) $(deps)

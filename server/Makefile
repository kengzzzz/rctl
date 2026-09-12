all:rctlser

FILE=$(wildcard *.c)
OBJ = $(FILE:.c=.o)

rctlser:$(OBJ) $(LIB)
	$(CC) $(CFLAGS) $^ $(SERVER_LDFLAGS) -o $@
	@if [ "$(LTO)" = "1" ] || [ "$(STATIC)" = "1" ]; then $(STRIP) --strip-all $@; fi

%.o:%.c
	$(CC) $(CFLAGS) -c $^ -o $@

FORCE:          
PHONY += FORCE
.PHONY: $(PHONY)

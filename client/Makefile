all:cmdline.h rctlcli

FILE=$(filter-out cmdline.c, $(wildcard *.c))
OBJ = $(FILE:.c=.o)

cmdline.h:arg.ggo
	gengetopt < arg.ggo

cmdline.c: cmdline.h
	@test -f $@

cli.o: cmdline.h

rctlcli:cmdline.c $(OBJ) $(LIB)
	$(CC) $(CFLAGS) $^ $(CLIENT_LDFLAGS) -o $@
	@if [ "$(LTO)" = "1" ] || [ "$(STATIC)" = "1" ]; then $(STRIP) --strip-all $@; fi

%.o:%.c
	$(CC) $(CFLAGS) -c $< -o $@

FORCE:          
PHONY += FORCE
.PHONY: $(PHONY)

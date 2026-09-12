TOPDIR = $(CURDIR)

all: rctlser rctlcli

CC?=gcc
AR?=ar
RANLIB?=ranlib
CFLAGS?=-Wall -Wno-unused-function -Wno-unused-value -Wno-unused-variable -Wno-unused-but-set-variable
CFLAGS+=-I. -I$(TOPDIR)/include
# CFLAGS+=-g -DDEBUG
CFLAGS+=-D_GNU_SOURCE
LDFLAGS+=-lpthread -lssl -lcrypto
SERVER_LDFLAGS?=$(LDFLAGS) -lreadline -lncurses
CLIENT_LDFLAGS?=$(LDFLAGS)
STRIP=strip
export CC AR RANLIB CFLAGS LDFLAGS SERVER_LDFLAGS CLIENT_LDFLAGS STRIP

ifeq ($(STATIC),1)
LDFLAGS+=-static
SERVER_LDFLAGS+=-static
CLIENT_LDFLAGS+=-static
endif

ifeq ($(LTO),1)
CFLAGS+=-O2 -flto -ffat-lto-objects -ffunction-sections -fdata-sections -fno-semantic-interposition
LDFLAGS+=-O2 -flto -Wl,--gc-sections -s
SERVER_LDFLAGS+=-O2 -flto -Wl,--gc-sections -s
CLIENT_LDFLAGS+=-O2 -flto -Wl,--gc-sections -s
AR=gcc-ar
RANLIB=gcc-ranlib
endif

LIBDIR=$(TOPDIR)/lib
LIB=$(LIBDIR)/rctl.a
export LIB
$(LIB):
	@$(MAKE) -C $(LIBDIR)

SERDIR=$(TOPDIR)/server
rctlser:$(LIB)
	@$(MAKE) -C $(SERDIR)

CLIDIR=$(TOPDIR)/client
rctlcli:$(LIB)
	@$(MAKE) -C $(CLIDIR)

clean:
	-@rm -rf $(CLIDIR)/cmdline*
	-@rm -rf `find . -name "*.o"`
	-@rm -rf `find . -name "*.a"` 
	-@rm -rf $(CLIDIR)/rctlcli $(SERDIR)/rctlser

install_server:
	-@mkdir -p /etc/rctl
	-@cp rctl_cert.pem rctl_priv.pem /etc/rctl

install_client:
	-@mkdir -p /etc/ssl/certs/
	-@cp wirelesser_ca.crt /etc/ssl/certs/

tags: FORCE
	@find  . -name "*.h" -o -name "*.c" -o -name "*.s" > cscope.files
	@cscope -bkq -i cscope.files
	@ctags -L cscope.files

.PHONY: clean FORCE

-include $(SRC:.c=.d)

%.d:cmdline.h %.c
	@set -e; rm -f $@; \
		$(CC) -MM $(CFLAGS) $< > $@.$$$$; \
		sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
		rm -f $@.$$$$

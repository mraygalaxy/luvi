CC             = @CC@
CFLAGS         = @CFLAGS@
INSTALL        = @INSTALL@
INSTALL_PROGRAM= @INSTALL_PROGRAM@
INSTALL_SCRIPT = @INSTALL_SCRIPT@
LIBS	       = @LIBS@
INCLUDE	       = @DEFS@
LDFLAGS	       = @LDFLAGS@
PROTOBUFCMD    = @PROTOBUF@

EXECUTABLES=luvi_master luvi_slave
PROTOSOURCES=luvi.pb-c.c
SOURCES=common.c luvi.h
EXTRA_DIST=$(SOURCES) master.c slave.c luvi.proto
all: $(EXECUTABLES)

luvi.pb-c.c: luvi.proto
	$(PROTOBUFCMD) --c_out=. luvi.proto

luvi_master: master.c $(SOURCES) $(PROTOSOURCES)
	$(CC) $(CFLAGS) -o luvi_master master.c $(INCLUDE) $(SOURCES) $(PROTOSOURCES) $(LIBS) $(LDFLAGS)

luvi_slave: slave.c $(SOURCES) $(PROTOSOURCES)
	$(CC) $(CFLAGS) -o luvi_slave slave.c $(INCLUDE) $(SOURCES) $(PROTOSOURCES) $(LIBS) $(LDFLAGS)

clean:
	rm -f $(EXECUTABLES) $(PROTOSOURCES) luvi.pb-c.h

install:
	$(INSTALL) -m 0755 luvi_master $(bindir)/luvi_master
	$(INSTALL) -m 0755 luvi_slave $(bindir)/luvi_slave

uninstall:
	rm -f $(bindir)/luvi_master
	rm -f $(bindir)/luvi_slave

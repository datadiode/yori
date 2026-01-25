BINARIES=ysigner.exe

!INCLUDE "..\config\common.mk"

LINKPDB=/Pdb:ysigner.pdb

BIN_OBJS=signer.obj

MOD_OBJS=msigner.obj

compile: $(BIN_OBJS) builtins.lib

ysigner.exe: $(BIN_OBJS) $(YORILIBS) $(YORIVER)
	@echo $@
	@$(LINK) $(LDFLAGS) -entry:$(YENTRY) $(BIN_OBJS) $(YORILIBS) $(EXTERNLIBS) advapi32.lib crypt32.lib $(YORIVER) -version:$(YORI_VER_MAJOR).$(YORI_VER_MINOR) $(LINKPDB) -out:$@

msigner.obj: signer.c
	@echo $@
	@$(CC) -c -DYORI_BUILTIN=0 $(CFLAGS) -Fo$@ signer.c

builtins.lib: $(MOD_OBJS)
	@echo $@
	@$(LIB32) $(LIBFLAGS) $(MOD_OBJS) -out:$@

WINDRES ?= windres

CFLAGS += -std=c99
LDLIBS += -lkernel32 -luser32 -lgdi32 -lcomdlg32

all: SC.exe

SC.exe: src/main.o src/compat.o src/MainWnd.o src/SheetCtrl.o src/SC.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

src/SC.o: src/SC.rc
	$(WINDRES) $(RESFLAGS) -o $@ $^	

clean:
	rm -f src/*.o SC.exe

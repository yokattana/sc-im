LDLIBS += -lkernel32 -luser32 -lgdi32 -lcomdlg32

all: SC.exe

SC.exe: src/main.o src/compat.o src/MainWnd.o src/SheetCtrl.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

clean:
	rm -f src/*.o SC.exe

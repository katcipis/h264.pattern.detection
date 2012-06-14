all: haar-test 

OPENCV_CFLAGS=`pkg-config --cflags opencv`
OPENCV_LFLAGS=`pkg-config --libs opencv` 

GLIB_CFLAGS=`pkg-config --cflags glib-2.0`
GLIB_LFLAGS=`pkg-config --libs glib-2.0`

CFLAGS=-O2 $(OPENCV_CFLAGS) $(GLIB_CFLAGS)
LDFLAGS=-L. $(OPENCV_LFLAGS) $(GLIB_LFLAGS)

haar-test: haar-test.c
	$(CC) $(CFLAGS) $? -o $@ $(LDFLAGS) 

clean:
	rm -rf haar-test *.o
	rm -rf *.*~

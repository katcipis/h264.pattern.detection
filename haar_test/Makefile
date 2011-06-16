all: haar-test 

OPENCV_CFLAGS=`pkg-config --cflags opencv`
OPENCV_LFLAGS=`pkg-config --libs opencv` 

CFLAGS=-O2 $(OPENCV_CFLAGS)
LDFLAGS=-L. $(OPENCV_LFLAGS)

haar-test: haar-test.c
	$(CC) $(CFLAGS) $(LDFLAGS) $? -o $@

clean:
	rm -rf haar-test *.o
	rm -rf *.*~

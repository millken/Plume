all:plm_echo.so
plm_echo.so:
	gcc -g -I../../lib plm_echo.c -L/usr/local/plume/lib -lplm_util -fPIC \
	-shared -o libplm_echo.so

clean:
	rm -rf *.o *.so
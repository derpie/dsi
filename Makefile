all: nus_get.c
	gcc nus_get.c -lcrypto -o nus_get

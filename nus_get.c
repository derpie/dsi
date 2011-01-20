/*
	Copyright 2011 derpie
	Released under the GNU GPLv2.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <openssl/aes.h>

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;

const char* cdn = "http://nus.cdn.t.shop.nintendowifi.net/ccs/download/";
const char common_key[] = {
	0xaf, 0x1b, 0xf5, 0x16, 0xa8, 0x07, 0xd2, 0x1a,
	0xea, 0x45, 0x98, 0x4f, 0x04, 0x74, 0x28, 0x61
};

char* title_version = NULL;
char* title_id = "";
char* tmd_buf = NULL;
char* cetk_buf = NULL;
char title_key[16];

u16 be16(u8* p) {
	return (p[0] << 8) | p[1];
}

u32 be32(u8 *p) {
	return (p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];
}

/* read file into allocated buf */
char* file_to_buf(char* file)
{
	FILE* fp = fopen(file, "rb");
	int len;
	char* buf;
	
	fseek(fp, 0, SEEK_END);
	len = ftell(fp);
	rewind(fp);
	
	buf = malloc(len);
	if(buf == NULL)
	{
		fclose(fp);
		return buf;
	}
	
	if(fread(buf, len, 1, fp) != 1)
	{
		free(buf);
		fclose(fp);
		return NULL;
	}
	
	fclose(fp);
	return buf;
}

void cdn_getfile(char* file, char* output_file)
{
	char buf[256];
	sprintf(buf, "wget --quiet --output-document=%s %s%s/%s",
		output_file, cdn, title_id, file);
	printf("%s\n", buf);
	if(system(buf) != 0)
	{
		printf("wget failed.\n");
		exit(1);
	}
}

void decrypt_titlekey()
{
	AES_KEY aes_key;
	char iv[16];
	
	memset(iv, 0, sizeof(iv));
	memcpy(iv, cetk_buf+0x1dc, 8);

	AES_set_decrypt_key(common_key, 128, &aes_key);
	AES_cbc_encrypt(cetk_buf+0x1bf, title_key, 16, &aes_key, iv, AES_DECRYPT);
}

void decrypt(char* file, int size, int n)
{
	char* buf = file_to_buf(file);
	char iv[16];
	AES_KEY aes_key;
	FILE* fp;
	
	memset(iv, 0, 16);
	memcpy(iv, tmd_buf+0x01e8 + 0x24*n, 2);
	
	AES_set_decrypt_key(title_key, 128, &aes_key);
	AES_cbc_encrypt(buf, buf, size, &aes_key, iv, AES_DECRYPT);
	
	fp = fopen(file, "wb+");
	
	if(fp == NULL)
	{
		printf("fopen : %s\n", strerror(errno));
		exit(1);
	}
	
	fwrite(buf, size, 1, fp);
	fclose(fp);
	free(buf);
}

void get_cetk()
{
	cdn_getfile("cetk", "cetk");
	cetk_buf = file_to_buf("cetk");

	if(cetk_buf == NULL)
	{
		printf("cetk failed to download.\n");
		exit(1);
	}
}

void get_tmd()
{
	char tmd_file[16];

	if(title_version != NULL)
		sprintf(tmd_file, "tmd.%s", title_version);
	else
		sprintf(tmd_file, "tmd");

	cdn_getfile(tmd_file, tmd_file);
	tmd_buf = file_to_buf(tmd_file);
	
	if(tmd_buf == NULL)
	{
		printf("tmd failed to download.\n");
		exit(1);
	}
}

void get_contents()
{
	int n_contents = be16(tmd_buf+0x1de);
	unsigned int cid;
	unsigned int size;
	char content_file[16];
	char output_file[16];
		
	int i;
	for(i=0; i<n_contents; i++)
	{
		printf("Content #%d:\n", i);
		cid = be32(tmd_buf+0x1e4 + 0x24*i);
		size = be32(tmd_buf+0x1e4+12 + 0x24*i);
		
		printf("  File: %08x\n", cid);
		printf("  Size: %d kbyte\n", size/1024);
		
		sprintf(content_file, "%08x", cid);
		sprintf(output_file, "%08x", i);
	
		printf("  Downloading..\n");
		cdn_getfile(content_file, output_file);
		printf("    OK!\n");
		
		printf("  Decrypting..\n");
		decrypt(output_file, size, i);
		printf("    OK!\n");
	}
}

int main(int argc, char* argv[])
{
	if((argc != 2) && (argc != 3))
	{
		printf("Usage: %s title-id <version>\n", argv[0]);
		return 0;
	}

	title_id = argv[1];
	printf("Title: %s\n", title_id);

	if(argc == 3) {
		title_version = argv[2];
		printf("Version: %s\n\n", title_version);
	}
	else {
		printf("Version: Latest\n\n");
	}

	/* new dir for title-id */
	if((mkdir(title_id, 0777) != 0) && (errno != EEXIST))
	{
		printf("mkdir: %s\n", strerror(errno));
		return 1;
	}

	if(chdir(title_id) != 0)
	{
		printf("chdir : %s\n", strerror(errno));
		return 1;
	}

	printf("Downloading TMD...\n");
	get_tmd();
	printf("  OK!\n\n");
	
	printf("Downloading CETK...\n");
	get_cetk();
	printf("  OK!\n\n");
	
	decrypt_titlekey();
	get_contents();
	
	free(tmd_buf);
	free(cetk_buf);
	
	printf("\nSuccess!\n");
	return 0;
}

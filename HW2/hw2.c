#define _GNU_SOURCE
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#define link(_x,x) \
if(_x == NULL){\
	_x = dlsym(RTLD_NEXT, x);\
}

#define LOGGER_FD 3

static int (*_chmod)(const char*, mode_t) = NULL;
static int (*_chown)(const char*, uid_t, gid_t) = NULL;
static int (*_close)(int) = NULL;
static int (*_creat)(const char*, int) = NULL;
static int (*_creat64)(const char*, int) = NULL;
static int (*_fclose)(FILE*) = NULL;
static FILE* (*_fopen)(const char*, const char*) = NULL;
static FILE* (*_fopen64)(const char*, const char*) = NULL;
static size_t (*_fread)(void*, size_t, size_t, FILE*) = NULL;
static size_t (*_fwrite)(const void*, size_t, size_t, FILE*) = NULL;
static int (*_open)(const char*, int, ...) = NULL;
static int (*_open64)(const char*, int, ...) = NULL;
static ssize_t (*_read)(int, void*, size_t) = NULL;
static int (*_remove)(const char*) = NULL;
static int (*_rename)(const char*, const char*) = NULL;
static FILE* (*_tmpfile)(void) = NULL;
static FILE* (*_tmpfile64)(void) = NULL;
static ssize_t (*_write)(int, const void*, size_t) = NULL;

void init(){
	link(_fwrite,"fwrite")
	link(_write,"write")
	link(_chmod,"chmod")
	link(_chown,"chown")
	link(_close,"close")
	link(_creat,"creat")
	link(_creat64,"creat64")
	link(_fclose,"fclose")
	link(_fopen,"fopen")
	link(_fopen64,"fopen64")
	link(_fread,"fread")
	link(_open,"open")
	link(_open64,"open64")
	link(_read,"read")
	link(_remove,"remove")
	link(_rename,"rename")
	link(_tmpfile,"tmpfile")
	link(_tmpfile64,"tmpfile64")
}

void fd2name(int fd, char *name){
	pid_t pid = getpid();
	char closed_fd[1024] = "";

	sprintf(closed_fd,"/proc/%d/fd/%d",pid,fd);
	
	int s = readlink(closed_fd,name,1024);

	if(s == -1){
		//fprintf(stderr, "fail to readlink %s\nerrno:%d\n", closed_fd,errno);
		strcpy(name,"");
	}
	return;
}

void FILE2name(FILE *f, char *name){
	int fd = fileno(f);
	fd2name(fd,name);
	return;
}
/*
int get_out_fd(){
	init();
	char *outputfile = NULL;
	outputfile = getenv("OUT");
	int fd_out = 3;
	
	if(outputfile == NULL)
		fd_out = fileno(stderr);
	else
		fd_out = 3;

	return fd_out;
}
*/
int open(const char *pathname, int oflags, ...){
	//int fd_out = get_out_fd();
	init();

	char *resolved_path = NULL;
	resolved_path = realpath(pathname,NULL);
	int fd = 0;
	va_list arg;
	mode_t mode = 0;

	va_start(arg,oflags);

	if ((oflags & O_CREAT)){
		mode = va_arg(arg, mode_t);
		fd = _open(pathname,oflags,mode);
	}
	else {
		fd = _open(pathname,oflags);
	}

	va_end(arg);

	dprintf(LOGGER_FD,"[logger] open(\"%s,\", %o, %o) = %d\n",(resolved_path)?resolved_path:pathname,oflags,mode,fd);

	return fd;
}

int open64(const char *pathname, int oflags, ...){
	//int fd_out = get_out_fd();
	init();

	char *resolved_path = NULL;
	resolved_path = realpath(pathname,NULL);
	int fd = 0;
	va_list arg;
	mode_t mode = 0;

	va_start(arg,oflags);

	if ((oflags & O_CREAT)){
		mode = va_arg(arg, mode_t);
		fd = _open64(pathname,oflags,mode);
	}
	else {
		fd = _open64(pathname,oflags);
	}

	va_end(arg);

	dprintf(LOGGER_FD,"[logger] open64(\"%s,\", %o, %o) = %d\n",(resolved_path)?resolved_path:pathname,oflags,mode,fd);

	return fd;
}

int close(int fd){
	//int fd_out = get_out_fd();
	init();

	char filename[1024] = "";
	
	fd2name(fd,filename);

	int ret = _close(fd);
	dprintf(LOGGER_FD, "[logger] close(\"%s\") = %d\n", filename, ret);
	

	return ret;
}

ssize_t read(int fd, void *buf, size_t count){
	//int fd_out = get_out_fd();
	init();

	char filename[1024] = "";
	char outbuf[33] = "";

	fd2name(fd,filename);

	ssize_t ret = _read(fd,buf,count);

	strncpy(outbuf, (char*)buf,32);
	outbuf[32] = '\0';
	int i = 0;
	for(i = 0; i < strlen(outbuf); i++){
		outbuf[i] = (isprint(outbuf[i]))? outbuf[i]:'.';
	}

	dprintf(LOGGER_FD,"[logger] read(\"%s\",\"%s\",%ld) = %ld\n", filename, outbuf, count, ret);

	
	return ret;
}

ssize_t write(int fd,const void *buf, size_t count){
	//int fd_out = get_out_fd();
	init();
	char filename[1024] = "";
	char outbuf[33] = "";

	fd2name(fd,filename);
	strncpy(outbuf,(char*)buf,32);
	outbuf[32] = '\0';
	int i = 0;
	for( i = 0; i < strlen(outbuf); i++){
		outbuf[i] = (isprint(outbuf[i]))? outbuf[i]:'.';
	}

	ssize_t ret = _write(fd,buf,count);

	dprintf(LOGGER_FD,"[logger] write(\"%s\",\"%s\",%ld) = %ld\n", filename, outbuf, count, ret);
	return ret;
}

int chmod(const char *pathname, mode_t mode){
	//int fd_out = get_out_fd();
	init();

	char *resolved_path = NULL;
	resolved_path = realpath(pathname,NULL);

	int ret = _chmod(pathname,mode);
	
	dprintf(LOGGER_FD,"[logger] chmod(\"%s\", %o) = %d\n", (resolved_path)?resolved_path:pathname, mode, ret);
	return ret;
}

int chown(const char *pathname, uid_t owner, gid_t group){
	//int fd_out = get_out_fd();
	init();
	char *resolved_path = NULL;
	resolved_path = realpath(pathname,NULL);

	int ret = _chown(pathname,owner,group);
	
	dprintf(LOGGER_FD,"[logger] chown(\"%s\", %d, %d) = %d\n", (resolved_path)?resolved_path:pathname, owner, group, ret);
	
	return ret;
}

int creat(const char *pathname, mode_t mode){
	//int fd_out = get_out_fd();
	init();

	int ret = _creat(pathname, mode);

	char *resolved_path = NULL;
	resolved_path = realpath(pathname,NULL);

	dprintf(LOGGER_FD,"[logger] creat(\"%s\", %o) = %d\n", (resolved_path)?resolved_path:pathname, mode, ret);
	
	return ret;
}

int creat64(const char *pathname, mode_t mode){
	//int fd_out = get_out_fd();
	init();

	int ret = _creat64(pathname, mode);

	char *resolved_path = NULL;
	resolved_path = realpath(pathname,NULL);

	dprintf(LOGGER_FD,"[logger] creat64(\"%s\", %o) = %d\n", (resolved_path)?resolved_path:pathname, mode, ret);
	
	return ret;
}

int fclose(FILE *stream){
	//int fd_out = get_out_fd();
	init();

	char filename[1024] = "";
	FILE2name(stream,filename);

	int ret = _fclose(stream);

	dprintf(LOGGER_FD, "[logger] fclose(\"%s\") = %d\n", filename, ret);
	
	return ret;
}

FILE *fopen(const char *pathname, const char *mode){
	//int fd_out = get_out_fd();
	init();

	FILE *ret = _fopen(pathname, mode);

	char *resolved_path = NULL;
	resolved_path = realpath(pathname,NULL);

	dprintf(LOGGER_FD, "[logger] fopen(\"%s\", \"%s\") = %p\n", (resolved_path)?resolved_path:pathname, mode, ret);
	
	return ret;
}

FILE *fopen64(const char *pathname, const char *mode){
	//int fd_out = get_out_fd();
	init();

	FILE *ret = _fopen64(pathname, mode);

	char *resolved_path = NULL;
	resolved_path = realpath(pathname,NULL);

	dprintf(LOGGER_FD, "[logger] fopen64(\"%s\", \"%s\") = %p\n", (resolved_path)?resolved_path:pathname, mode, ret);
	
	return ret;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream){
	//int fd_out = get_out_fd();
	init();

	char filename[1024] = "";
	FILE2name(stream,filename);

	size_t ret = _fread(ptr,size,nmemb,stream);

	char outbuf[33] = "";

	int i = 0;
	strncpy(outbuf, (char*)ptr, 32);
	outbuf[32] = '\0';
	for(i = 0; i < strlen(outbuf); i++){
		outbuf[i] = (isprint(outbuf[i]))? outbuf[i]:'.';
	}

	dprintf(LOGGER_FD, "[logger] fread(\"%s\", %ld, %ld, \"%s\") = %ld\n", outbuf, size, nmemb, filename, ret);
	
	return ret;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream){
	//int fd_out = get_out_fd();
	init();

	char filename[1024] = "";
	FILE2name(stream,filename);

	size_t ret = _fwrite(ptr,size,nmemb,stream);

	char outbuf[33] = "";

	int i = 0;
	strncpy(outbuf, (char*)ptr, 32);
	outbuf[32] = '\0';
	for(i = 0; i < strlen(outbuf); i++){
		outbuf[i] = (isprint(outbuf[i]))? outbuf[i]:'.';
	}

	dprintf(LOGGER_FD, "[logger] fwrite(\"%s\", %ld, %ld, \"%s\") = %ld\n", outbuf, size, nmemb, filename, ret);
	
	return ret;
}

int remove(const char *pathname){
	//int fd_out = get_out_fd();
	init();

	char *resolved_path = NULL;
	resolved_path = realpath(pathname,NULL);

	int ret = _remove(pathname);
	
	dprintf(LOGGER_FD,"[logger] remove(\"%s\") = %d\n", (resolved_path)?resolved_path:pathname, ret);
	
	return ret;
}

int rename(const char *oldpath, const char *newpath){
	//int fd_out = get_out_fd();
	init();

	char *resolved_oldpath = NULL;
	resolved_oldpath = realpath(oldpath,NULL);

	int ret = _rename(oldpath,newpath);

	char *resolved_newpath = NULL;
	resolved_newpath = realpath(newpath,NULL);

	dprintf(LOGGER_FD, "[logger] rename(\"%s\", \"%s\") = %d\n", (resolved_oldpath)?resolved_oldpath:oldpath, (resolved_newpath)?resolved_newpath:newpath, ret);

	return ret;
}

FILE *tmpfile(void){
	//int fd_out = get_out_fd();
	init();

	FILE *ret = _tmpfile();
	
	dprintf(LOGGER_FD, "[logger] tmpfile() = %p\n", ret);

	
	return ret;
}

FILE *tmpfile64(void){
	//int fd_out = get_out_fd();
	init();

	FILE *ret = _tmpfile64();
	
	dprintf(LOGGER_FD, "[logger] tmpfile64() = %p\n", ret);

	
	return ret;
}

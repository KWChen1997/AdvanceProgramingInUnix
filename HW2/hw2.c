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

static int (*_chmod)(const char*, mode_t) = NULL;
static int (*_chown)(const char*, uid_t, gid_t) = NULL;
static int (*_close)(int) = NULL;
static int (*_creat)(const char*, int) = NULL;
static int (*_fclose)(FILE*) = NULL;
static FILE* (*_fopen)(const char*, const char*) = NULL;
static size_t (*_fread)(void*, size_t, size_t, FILE*) = NULL;
static size_t (*_fwrite)(const void*, size_t, size_t, FILE*) = NULL;
static int (*_open)(const char*, int, ...) = NULL;
static ssize_t (*_read)(int, void*, size_t) = NULL;
static int (*_remove)(const char*) = NULL;
static int (*_rename)(const char*, const char*) = NULL;
static FILE* (*_tmpfile)(void) = NULL;
static ssize_t (*_write)(int, const void*, size_t) = NULL;

void get_chmod(){
	if(_chmod == NULL){
		void *handle = dlopen("libc.so.6", RTLD_LAZY);
		if(handle != NULL){
			_chmod = dlsym(handle, "chmod");
			dlclose(handle);
		}
	}
	return;
}

void get_chown(){
	if(_chown == NULL){
		void *handle = dlopen("libc.so.6", RTLD_LAZY);
		if(handle != NULL){
			_chown = dlsym(handle, "chown");
			dlclose(handle);
		}
	}
	return;
}

void get_close(){
	if(_close == NULL){
		void *handle = dlopen("libc.so.6", RTLD_LAZY);
		if(handle != NULL){
			_close = dlsym(handle, "close");
			dlclose(handle);
		}
	}
	return;
}

void get_creat(){
	if(_creat == NULL){
		void *handle = dlopen("libc.so.6", RTLD_LAZY);
		if(handle != NULL){
			_creat = dlsym(handle, "creat");
			dlclose(handle);
		}
	}
	return;
}

void get_fclose(){
	if(_fclose == NULL){
		void *handle = dlopen("libc.so.6", RTLD_LAZY);
		if(handle != NULL){
			_fclose = dlsym(handle, "fclose");
			dlclose(handle);
		}
	}
	return;
}

void get_fopen(){
	if(_fopen == NULL){
		void *handle = dlopen("libc.so.6", RTLD_LAZY);
		if(handle != NULL){
			_fopen = dlsym(handle, "fopen");
			dlclose(handle);
		}
	}
	return;
}

void get_fread(){
	if(_fread == NULL){
		void *handle = dlopen("libc.so.6", RTLD_LAZY);
		if(handle != NULL){
			_fread = dlsym(handle, "fread");
			dlclose(handle);
		}
	}
	return;
}

void get_fwrite(){
	if(_fwrite == NULL){
		void *handle = dlopen("libc.so.6", RTLD_LAZY);
		if(handle != NULL){
			_fwrite = dlsym(handle, "fwrite");
			dlclose(handle);
		}
	}
	return;
}

void get_open(){
	if(_open == NULL){
		void *handle = dlopen("libc.so.6", RTLD_LAZY);
		if(handle != NULL){
			_open = dlsym(handle, "open");
			dlclose(handle);
		}
	}
	return;
}

void get_read(){
	if(_read == NULL){
		void *handle = dlopen("libc.so.6", RTLD_LAZY);
		if(handle != NULL){
			_read = dlsym(handle, "read");
			dlclose(handle);
		}
	}
	return;
}

void get_remove(){
	if(_remove == NULL){
		void *handle = dlopen("libc.so.6", RTLD_LAZY);
		if(handle != NULL){
			_remove = dlsym(handle, "remove");
			dlclose(handle);
		}
	}
	return;
}

void get_rename(){
	if(_rename == NULL){
		void *handle = dlopen("libc.so.6", RTLD_LAZY);
		if(handle != NULL){
			_rename = dlsym(handle, "rename");
			dlclose(handle);
		}
	}
	return;
}

void get_tmpfile(){
	if(_tmpfile == NULL){
		void *handle = dlopen("libc.so.6", RTLD_LAZY);
		if(handle != NULL){
			_tmpfile = dlsym(handle, "tmpfile");
			dlclose(handle);
		}
	}
	return;
}

void get_write(){
	if(_write == NULL){
		void *handle = dlopen("libc.so.6", RTLD_LAZY);
		if(handle != NULL){
			_write = dlsym(handle, "write");
			dlclose(handle);
		}
	}
	return;
}

void get_all(){
	get_fwrite();
	get_write();
	get_chmod();
	get_chown();
	get_close();
	get_creat();
	get_fclose();
	get_fopen();
	get_fread();
	get_open();
	get_read();
	get_remove();
	get_rename();
	get_tmpfile();
}

void fd2name(int fd, char *name){
	pid_t pid = getpid();
	char closed_fd[1024] = "";

	sprintf(closed_fd,"/proc/%d/fd/%d",pid,fd);
	
	int s = readlink(closed_fd,name,1024);
	


	if(s == -1){
		fprintf(stderr, "fail to readlink %s\nerrno:%d\n", closed_fd,errno);
		strcpy(name,"");
	}
	return;
}

void FILE2name(FILE *f, char *name){
	int fd = fileno(f);
	fd2name(fd,name);
	return;
}

int get_out_fd(){
	get_all();
	char *outputfile = NULL;
	outputfile = getenv("OUT");
	int fd_out = 2;
	
	if(outputfile == NULL)
		fd_out = 2;
	else
		fd_out = _open(outputfile,O_CREAT|O_WRONLY|O_APPEND, 0600);

	return fd_out;
}

void close_fd_out(int fd){
	get_all();
	if(fd != 2){
		_close(fd);
	}
	return;
}

int open(const char *pathname, int oflags, ...){
	int fd_out = get_out_fd();
	get_all();

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

	dprintf(fd_out,"[logger] open(\"%s,\", %04d, %o) = %d\n",(resolved_path)?resolved_path:pathname,oflags,mode,fd);

	close_fd_out(fd_out);
	return fd;
}


int close(int fd){
	int fd_out = get_out_fd();
	get_all();

	char filename[1024] = "";
	
	fd2name(fd,filename);

	int ret = _close(fd);
	dprintf(fd_out, "[logger] close(\"%s\") = %d\n", filename, ret);
	
	close_fd_out(fd_out);

	return ret;
}

ssize_t read(int fd, void *buf, size_t count){
	int fd_out = get_out_fd();
	get_all();

	char filename[1024] = "";
	char outbuf[33] = "";

	fd2name(fd,filename);

	ssize_t ret = _read(fd,buf,count);

	strncpy(outbuf, (char*)buf,32);
	outbuf[32] = '\0';
	int i = 0;
	for(i = 0; i < 33 && outbuf[i] != '\0'; i++){
		if(!isprint(outbuf[i]))
			outbuf[i] = '.';
	}

	dprintf(fd_out,"[logger] read(\"%s\",\"%s\",%ld) = %ld\n", filename, outbuf, count, ret);

	close_fd_out(fd_out);
	return ret;
}

ssize_t write(int fd,const void *buf, size_t count){
	int fd_out = get_out_fd();
	get_all();
	char filename[1024] = "";
	char outbuf[33] = "";

	fd2name(fd,filename);
	strncpy(outbuf,(char*)buf,32);
	outbuf[32] = '\0';
	int i = 0;
	for( i = 0; i < 33 && outbuf[i] != '\0'; i++){
		if(!isprint(outbuf[i]))
			outbuf[i] = '.';
	}

	ssize_t ret = _write(fd,buf,count);

	dprintf(fd_out,"[logger] write(\"%s\",\"%s\",%ld) = %ld\n", filename, outbuf, count, ret);
	close_fd_out(fd_out);
	return ret;
}

int chmod(const char *pathname, mode_t mode){
	int fd_out = get_out_fd();
	get_all();

	char *resolved_path = NULL;
	resolved_path = realpath(pathname,NULL);

	int ret = _chmod(pathname,mode);
	
	dprintf(fd_out,"[logger] chmod(\"%s\", %o) = %d\n", (resolved_path)?resolved_path:pathname, mode, ret);
	close_fd_out(fd_out);
	return ret;
}

int chown(const char *pathname, uid_t owner, gid_t group){
	int fd_out = get_out_fd();
	get_all();
	char *resolved_path = NULL;
	resolved_path = realpath(pathname,NULL);

	int ret = _chown(pathname,owner,group);
	
	dprintf(fd_out,"[logger] chown(\"%s\", %d, %d) = %d\n", (resolved_path)?resolved_path:pathname, owner, group, ret);
	close_fd_out(fd_out);
	return ret;
}

int creat(const char *pathname, mode_t mode){
	int fd_out = get_out_fd();
	get_all();

	int ret = _creat(pathname, mode);

	char *resolved_path = NULL;
	resolved_path = realpath(pathname,NULL);

	dprintf(fd_out,"[logger] creat(\"%s\", %o) = %d\n", (resolved_path)?resolved_path:pathname, mode, ret);
	close_fd_out(fd_out);
	return ret;
}

int fclose(FILE *stream){
	int fd_out = get_out_fd();
	get_all();

	char filename[1024] = "";
	FILE2name(stream,filename);

	int ret = _fclose(stream);

	dprintf(fd_out, "[logger] fclose(\"%s\") = %d\n", filename, ret);
	close_fd_out(fd_out);
	return ret;
}

FILE *fopen(const char *pathname, const char *mode){
	int fd_out = get_out_fd();
	get_all();

	FILE *ret = _fopen(pathname, mode);

	char *resolved_path = NULL;
	resolved_path = realpath(pathname,NULL);

	dprintf(fd_out, "[logger] fopen(\"%s\", \"%s\") = %p\n", (resolved_path)?resolved_path:pathname, mode, ret);
	close_fd_out(fd_out);
	return ret;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream){
	int fd_out = get_out_fd();
	get_all();

	char filename[1024] = "";
	FILE2name(stream,filename);

	size_t ret = _fread(ptr,size,nmemb,stream);

	dprintf(fd_out, "[logger] fread(\"%s\", %ld, %ld, \"%s\") = %ld\n", (char*)ptr, size, nmemb, filename, ret);
	close_fd_out(fd_out);
	return ret;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream){
	int fd_out = get_out_fd();
	get_all();

	char filename[1024] = "";
	FILE2name(stream,filename);

	size_t ret = _fwrite(ptr,size,nmemb,stream);

	dprintf(fd_out, "[logger] fwrite(\"%s\", %ld, %ld, \"%s\") = %ld\n", (char*)ptr, size, nmemb, filename, ret);
	close_fd_out(fd_out);
	return ret;
}

int remove(const char *pathname){
	int fd_out = get_out_fd();
	get_all();

	char *resolved_path = NULL;
	resolved_path = realpath(pathname,NULL);

	int ret = _remove(pathname);
	
	dprintf(fd_out,"[logger] remove(\"%s\") = %d\n", (resolved_path)?resolved_path:pathname, ret);
	close_fd_out(fd_out);
	return ret;
}

int rename(const char *oldpath, const char *newpath){
	int fd_out = get_out_fd();
	get_all();

	char *resolved_oldpath = NULL;
	resolved_oldpath = realpath(oldpath,NULL);

	int ret = _rename(oldpath,newpath);

	char *resolved_newpath = NULL;
	resolved_newpath = realpath(newpath,NULL);

	dprintf(fd_out, "[logger] rename(\"%s\", \"%s\") = %d\n", (resolved_oldpath)?resolved_oldpath:oldpath, (resolved_newpath)?resolved_newpath:newpath, ret);

	return ret;
}

FILE *tmpfile(void){
	int fd_out = get_out_fd();
	get_all();

	FILE *ret = _tmpfile();
	
	dprintf(fd_out, "[logger] tmpfile() = %p\n", ret);

	close_fd_out(fd_out);
	return ret;
}

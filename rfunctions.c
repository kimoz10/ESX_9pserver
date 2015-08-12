/*
 * rfunctions.c
 *
 *  Created on: Jul 17, 2015
 *      Author: kelghamrawy
 */
#include "9p.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <libgen.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <strings.h>
#include "objLib.h"

int permissions(struct stat *st){
	int permissions;
	permissions = 0;
	if(st->st_mode & S_IRUSR) permissions|=0b100000000;
	if(st->st_mode & S_IWUSR) permissions|=0b010000000;
	if(st->st_mode & S_IXUSR) permissions|=0b001000000;
	if(st->st_mode & S_IRGRP) permissions|=0b000100000;
	if(st->st_mode & S_IWGRP) permissions|=0b000010000;
	if(st->st_mode & S_IXGRP) permissions|=0b000001000;
	if(st->st_mode & S_IROTH) permissions|=0b000000100;
	if(st->st_mode & S_IWOTH) permissions|=0b000000010;
	if(st->st_mode & S_IXOTH) permissions|=0b000000001;
	return permissions;
}
void UNIX_stat_to_qid(struct stat *st, qid_t *qid){
	qid->path = st->st_ino;
	qid->version = (st->st_mtime ^ (st->st_size << 8)) & 0; /* may be this will need to reflect any modifications to the file. zero for now */
	qid->type = 0;
	if (S_ISDIR(st->st_mode)) qid->type |= QTDIR;
	if (S_ISLNK(st->st_mode)) qid->type |= QTSYMLINK;
}

void UNIX_stat_to_stat(char* filename, struct stat *st, stat_t *s){
	assert(st != NULL);
	s->name = (char *) malloc(50);
	s->name = strncpy(s->name, filename, 49);
	s->atime = st->st_atime;
	s->mtime = st->st_mtime;
	s->length = st->st_size;
	s->type = 0;
	s->dev = 0;
	s->qid = (qid_t *) malloc(sizeof(qid_t));
	UNIX_stat_to_qid(st, s->qid);
	s -> uid = (char *) malloc(7);
	s -> gid = (char *) malloc(7);
	strcpy(s->uid, "nobody");
	strcpy(s->gid, "nobody");
	/* dont forget to assign muid */
	s -> muid = "";
	/* setting up the mode for the stat */
	s->mode = ((((uint32_t) s->qid->type & 0x80) << 24) | permissions(st));
	/* allowing all permissions for now since eventually it is going to run internally in ESX which can be considered relatively trusted
	 * but this had to change later */
}

void make_qid_from_UNIX_file(const char *pathname, qid_t *qid){
	struct stat *st;
	st = (struct stat *) malloc(sizeof(struct stat));
	lstat(pathname, st);
	UNIX_stat_to_qid(st, qid);
	/* NEW */
	free(st);
}

void make_stat_from_UNIX_file(char *pathname, stat_t *s){
	struct stat *st;
	char *filename;
	st  = (struct stat *) malloc(sizeof(struct stat));
	lstat(pathname, st);
	filename = strrchr(pathname, '/');
	if(filename) filename = filename + 1;
	else filename = pathname;
	UNIX_stat_to_stat(filename, st, s);
	/* NEW */
	free(st);
}

int is_file_exists(char *newpathname){
	struct stat buffer;
	return (lstat(newpathname, &buffer) == 0);
}

void create_directory(char *pathname, char* filename){
	char *s;
	s = (char *)malloc(1000 * sizeof(char));
	bzero(s, 1000);
	strcat(s, pathname);
	strcat(s, "/");
	strcat(s,filename);
#ifdef DEBUG
	printf("trying to create directory %s\n", s);
#endif
	errno = 0;
	/* TODO: ADD appropriate permissions */
	if(mkdir(s, 0755)!=0){
		printf("error number %d\n", errno);
		exit(1);
	}
	free(s);
}

void create_file(char *pathname, char* filename, int perm){
	char *s;
	int f;
	s = (char *)malloc(1000 * sizeof(char));
	bzero(s, 1000);
	strcat(s, pathname);
	strcat(s, "/");
	strcat(s,filename);
	f = open(s, O_CREAT|O_RDWR, perm);
	close(f);
	free(s);
}

void UNIX_rename_directory(char *path, char *new_name){
	char *new_path;
	new_path = (char *) malloc(1000 * sizeof(char));
	bzero(new_path, 1000);
	strcat(new_path, path);
	strcat(new_path, "../");
	strcat(new_path, new_name);
	rename(path, new_path);
	free(new_path);
}

void UNIX_rename_file(char *path, char *new_name){
	char *last;
	char *new_path;
	int len;
	last = strrchr(path, '/');
	len = (int)(last - path + 1);
	new_path = (char *) malloc(1000 * sizeof(char));
	bzero(new_path, 1000);
	strncat(new_path, path, len);
	strcat(new_path, new_name);
	assert(rename(path, new_path) == 0);
	free(new_path);
}

void UNIX_change_permissions(char *path, uint32_t mode){
	/* TODO: handle this later */

	mode = mode & 0x000001ff;
	chmod(path, mode);

}

int UNIX_read(int fd, uint8_t *data, unsigned long long offset, unsigned int count){
	lseek(fd, offset, SEEK_SET);
	return read(fd, data, count);
}


int UNIX_write(int fd, unsigned long long offset, uint8_t *data, unsigned int count){
	int n;
	lseek(fd, offset, SEEK_SET);
	n = write(fd, data, count);
	return n;
}

int ESX_read(ObjHandle *handleID, uint8_t *buffer, uint64_t offset, unsigned int count){
	int val;
	ObjLibError err;
	uint64_t file_size;
	uint32_t actual_count;
        val = -1;
	ObjLib_GetSize(*handleID, &file_size);
	actual_count = MIN(file_size - offset, count);
#ifdef DEBUG
	printf("file size is %lu. count is %u. offset is %lu\n", file_size, count, offset);
#endif
	err = ObjLib_Pread(*handleID, (void *) buffer, actual_count, offset);
	if(ObjLib_IsSuccess(err)){
		/// should return number of bytes read
		val = actual_count;
	}
	else{
		printf("Error: %s\n", ObjLib_Err2String(err));
		val = -1;
		exit(1);
	}
	return val;	
}

int ESX_write(ObjHandle *handleID, uint64_t offset, uint8_t *data, unsigned int count){
	ObjLibError err;
	err = ObjLib_Pwrite(*handleID, (void *)data, count, offset);
	if(ObjLib_IsSuccess(err)){
		//// should return the number of bytes written
		return count;
	}
	else{
		return -1;
	}
}

int UNIX_remove(char *path){
	return remove(path);
}

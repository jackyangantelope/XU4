/* SPDX-License-Identifier: BSD-3-Clause
 * Newlib stdio/FatFS adapter. Core0 owns all files. */
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "pico/stdlib.h"
#include "hardware/psram.h"
#include "ff.h"
#include "xu4_profile.h"
#include "xu4_boot_diag.h"
#include "xu4_read_diag.h"
#include "xu4_integrity.h"
#include "xu4_close_diag.h"
static FIL *files[24];
static int file_flags[24];
#ifndef NATIVE_GAME_DIRECTORY
#define NATIVE_GAME_DIRECTORY "/roms/XU4"
#endif
static char cwd[256]=NATIVE_GAME_DIRECTORY;
static int fail(FRESULT r){
    errno=r==FR_NO_FILE||r==FR_NO_PATH?ENOENT:r==FR_EXIST?EEXIST:
        r==FR_WRITE_PROTECTED?EROFS:r==FR_DENIED?EACCES:r==FR_INVALID_NAME?EINVAL:EIO;
    return -1;
}
static int path_join(const char *name,char *out,size_t cap){
    if(!name||!*name){errno=ENOENT;return -1;}
    int n=snprintf(out,cap,name[0]=='/'?"%s":"%s/%s",name[0]=='/'?name:cwd,name);
    if(n<0||(size_t)n>=cap){errno=ENAMETOOLONG;return -1;}return 0;
}
int _open(const char *name,int flags,...){
    int slot;for(slot=3;slot<24&&files[slot];slot++);
    if(slot==24){errno=EMFILE;return -1;}
    char path[512];if(path_join(name,path,sizeof path))return -1;
    BYTE mode=(flags&O_ACCMODE)==O_RDONLY?FA_READ:(flags&O_ACCMODE)==O_WRONLY?FA_WRITE:FA_READ|FA_WRITE;
    if(flags&O_CREAT)mode|=(flags&O_EXCL)?FA_CREATE_NEW:(flags&O_TRUNC)?FA_CREATE_ALWAYS:FA_OPEN_ALWAYS;
    FIL *f=calloc(1,sizeof *f);if(!f){errno=ENOMEM;return -1;}
    xu4_prof_enter(XPROF_OPEN);
    unsigned boot_old=xu4_boot_wait(1,slot);
    const int read_diag=xu4_read_active();
    const uint64_t read_at=read_diag?time_us_64():0;
    FRESULT r=f_open(f,path,mode);
    if(read_diag)xu4_read_open(r,time_us_64()-read_at,r==FR_OK?f_size(f):0);
    xu4_boot_unwait(boot_old);
    xu4_prof_leave(XPROF_OPEN);
    if(r!=FR_OK){free(f);return fail(r);}
    if((flags&O_TRUNC)&&!(flags&O_CREAT)){r=f_truncate(f);}
    if(r==FR_OK&&(flags&O_APPEND))r=f_lseek(f,f_size(f));
    if(r!=FR_OK){f_close(f);free(f);return fail(r);}
    files[slot]=f;file_flags[slot]=flags;xu4_integrity_fd_open(slot,name);return slot;
}
int _close(int fd){
    if(fd<3||fd>=24||!files[fd]){errno=EBADF;return -1;}
    const int marked=xu4_integrity_is_fd(fd);
    unsigned boot_old=xu4_boot_wait(8,fd);
    xu4_close_mark(20,(uintptr_t)files[fd],marked);
    FRESULT r=f_close(files[fd]);
    xu4_close_mark(21,(uintptr_t)files[fd],marked);
    xu4_close_mark(30,(uintptr_t)files[fd],marked);
    free(files[fd]);
    xu4_close_mark(31,(uintptr_t)files[fd],marked);
    xu4_boot_unwait(boot_old);files[fd]=NULL;
    xu4_integrity_fd_close(fd);
    xu4_close_mark(22,(uintptr_t)fd,marked);
    return r==FR_OK?0:fail(r);
}
int _read(int fd,char *buf,int count){
    if(fd<3||fd>=24||!files[fd]||count<0){errno=EBADF;return -1;}
    xu4_prof_enter(XPROF_READ);
    unsigned boot_old=xu4_boot_wait(2,count);
    const int read_diag=xu4_read_active();
    const unsigned read_pos=f_tell(files[fd]);
    const uint64_t read_at=read_diag?time_us_64():0;
    UINT n=0;FRESULT r=f_read(files[fd],buf,count,&n);
    if(read_diag)xu4_read_record(count,n,r,time_us_64()-read_at,read_pos);
    xu4_integrity_fd_read(fd,buf,n,read_pos,r);
    xu4_boot_unwait(boot_old);
    xu4_prof_leave(XPROF_READ);
    return r==FR_OK?(int)n:fail(r);
}
int _write(int fd,char *buf,int count){
    if((fd==1||fd==2)&&count>=0){
        stdio_put_string(buf,count,false,true);
        return count;
    }
    if(fd<3||fd>=24||!files[fd]||count<0){errno=EBADF;return -1;}
    FRESULT r=FR_OK;
    if(file_flags[fd]&O_APPEND)r=f_lseek(files[fd],f_size(files[fd]));
    if(r!=FR_OK)return fail(r);
    UINT n=0;r=f_write(files[fd],buf,count,&n);return r==FR_OK?(int)n:fail(r);
}
off_t _lseek(int fd,off_t offset,int whence){
    if(fd<3||fd>=24||!files[fd]){errno=EBADF;return -1;}
    int64_t base=whence==SEEK_SET?0:whence==SEEK_CUR?f_tell(files[fd]):whence==SEEK_END?f_size(files[fd]):-1;
    int64_t to=base+(int64_t)offset;
    if(base<0||to<0||to>INT32_MAX){errno=EINVAL;return -1;}
    xu4_prof_enter(XPROF_SEEK);
    unsigned boot_old=xu4_boot_wait(3,fd);
    const int read_diag=xu4_read_active();
    const unsigned read_from=read_diag?f_tell(files[fd]):0;
    const uint64_t read_at=read_diag?time_us_64():0;
    FRESULT r=f_lseek(files[fd],(FSIZE_t)to);
    if(read_diag)xu4_read_seek(r,time_us_64()-read_at,read_from,(unsigned)to);
    xu4_boot_unwait(boot_old);
    xu4_prof_leave(XPROF_SEEK);
    return r==FR_OK?(off_t)f_tell(files[fd]):fail(r);
}
int _fstat(int fd,struct stat *s){
    memset(s,0,sizeof *s);
    if(fd>=0&&fd<3){s->st_mode=S_IFCHR;return 0;}
    if(fd<3||fd>=24||!files[fd]){errno=EBADF;return -1;}
    s->st_mode=S_IFREG|0666;s->st_size=f_size(files[fd]);return 0;
}
int _stat(const char *name,struct stat *s){
    char path[512];if(path_join(name,path,sizeof path))return -1;
    if(!strcmp(path,"/")){memset(s,0,sizeof *s);s->st_mode=S_IFDIR|0777;return 0;}
    unsigned boot_old=xu4_boot_wait(4,0);
    FILINFO info;FRESULT r=f_stat(path,&info);
    xu4_boot_unwait(boot_old);if(r!=FR_OK)return fail(r);
    memset(s,0,sizeof *s);s->st_mode=(info.fattrib&AM_DIR?S_IFDIR:S_IFREG)|0777;s->st_size=info.fsize;return 0;
}
int mkdir(const char *name,mode_t mode){
    (void)mode;char path[512];if(path_join(name,path,sizeof path))return -1;
    FRESULT r=f_mkdir(path);return r==FR_OK?0:fail(r);
}
int chdir(const char *name){
    char path[256];if(path_join(name,path,sizeof path))return -1;
    unsigned boot_old=xu4_boot_wait(4,0);
    FILINFO info;FRESULT r=f_stat(path,&info);
    xu4_boot_unwait(boot_old);if(r!=FR_OK)return fail(r);
    if(!(info.fattrib&AM_DIR)){errno=ENOTDIR;return -1;}strcpy(cwd,path);return 0;
}
char *getcwd(char *buffer,size_t size){
    size_t required=strlen(cwd)+1;
    if(size&&size<required){errno=ERANGE;return NULL;}
    if(!buffer){if(size==0)size=required;buffer=malloc(size);if(!buffer){errno=ENOMEM;return NULL;}}
    if(size<required){errno=ERANGE;return NULL;}
    memcpy(buffer,cwd,required);return buffer;
}
int _unlink(const char *name){char path[512];if(path_join(name,path,sizeof path))return -1;FRESULT r=f_unlink(path);return r==FR_OK?0:fail(r);}
int fsync(int fd){if(fd<3||fd>=24||!files[fd]){errno=EBADF;return -1;}FRESULT r=f_sync(files[fd]);return r==FR_OK?0:fail(r);}
int ftruncate(int fd,off_t size){
    if(fd<3||fd>=24||!files[fd]||size<0){errno=EINVAL;return -1;}
    FSIZE_t old=f_tell(files[fd]);FRESULT r=f_lseek(files[fd],size);
    if(r==FR_OK)r=f_truncate(files[fd]);
    if(r==FR_OK)r=f_lseek(files[fd],old);return r==FR_OK?0:fail(r);
}

extern char __psram_end__;
static uintptr_t heap_start,heap_break;
void *_sbrk(int amount){
    xu4_boot_heap(heap_break,amount,0);
    if(!psram_is_available()){xu4_boot_heap(heap_break,amount,1);errno=ENOMEM;return (void*)-1;}
    uintptr_t limit=0x11000000u+psram_get_size();
    if(!heap_start)heap_start=heap_break=((uintptr_t)&__psram_end__+7u)&~(uintptr_t)7u;
    if(heap_start<0x11000000u||heap_start>limit){errno=ENOMEM;return (void*)-1;}
    if((amount>0&&(uintptr_t)amount>limit-heap_break)||(amount<0&&(uint64_t)-(int64_t)amount>heap_break-heap_start)){
        xu4_boot_heap(heap_break,amount,1);errno=ENOMEM;return (void*)-1;
    }
    void *old=(void*)heap_break;heap_break+=amount;xu4_boot_heap(heap_break,amount,0);return old;
}

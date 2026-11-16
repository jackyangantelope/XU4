#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void xu4_integrity_start(const char *root);
void xu4_integrity_finish(void);
int xu4_integrity_is_fd(int fd);
int xu4_integrity_is_xml(void *ctx);
void xu4_integrity_fd_open(int fd,const char *name);
void xu4_integrity_fd_read(int fd,const void *data,unsigned n,unsigned pos,unsigned error);
void xu4_integrity_fd_close(int fd);
void xu4_integrity_xml_open(void *ctx,const char *name);
void xu4_integrity_xml_read(void *ctx,const void *data,int n);
void xu4_integrity_xml_close(void *ctx);
#ifdef __cplusplus
}
#endif

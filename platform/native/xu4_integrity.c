#include "xu4_integrity.h"
#include "xu4_boot_diag.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#define GRAPHICS_CRC32 0x877e510eu
static unsigned enabled;
static int graphics_fd=-1;
static void *graphics_ctx;
struct Check {uint32_t bytes,errors,crc;uintptr_t address;};
static struct Check raw_check,xml_check;
static void init(struct Check *c){memset(c,0,sizeof *c);c->crc=0xffffffffu;}
static void check(struct Check *c,const void *data,unsigned count){
 const unsigned char *p=data;
 c->address=(uintptr_t)data;
 c->bytes+=count;
 while(count--){
  c->crc^=*p++;
  for(unsigned bit=0;bit<8;++bit)c->crc=(c->crc>>1)^(0xedb88320u & (0u-(c->crc&1)));
 }
}
static void report(const char *stage,const struct Check *c){
 printf("XU4_DATA stage=%s pass=%u bytes=%lu errors=%lu crc32=%08lx ref_crc32=%08lx address=%08lx\n",stage,!c->errors&&(c->crc^0xffffffffu)==GRAPHICS_CRC32,(unsigned long)c->bytes,(unsigned long)c->errors,(unsigned long)(c->crc^0xffffffffu),(unsigned long)GRAPHICS_CRC32,(unsigned long)c->address);
}
static int is_graphics(const char *name){const char *p=name?strrchr(name,'/'):NULL;p=p?p+1:name;return p && !strcmp(p,"graphics.xml");}
void xu4_integrity_start(const char *root){
 (void)root;enabled=1;graphics_fd=-1;graphics_ctx=NULL;
 puts("XU4_DATA_BOOT build=close1 preflight=OFF temporary_psram=0 reference=crc32");
}
int xu4_integrity_is_fd(int fd){return enabled && fd==graphics_fd;}
int xu4_integrity_is_xml(void *ctx){return enabled && ctx && ctx==graphics_ctx;}
void xu4_integrity_fd_open(int fd,const char *name){if(enabled && is_graphics(name)){graphics_fd=fd;init(&raw_check);}}
void xu4_integrity_fd_read(int fd,const void *data,unsigned n,unsigned pos,unsigned error){
 if(!enabled || fd!=graphics_fd)return;
 (void)pos;check(&raw_check,data,n);raw_check.errors+=error!=0;
}
void xu4_integrity_fd_close(int fd){if(enabled && fd==graphics_fd){report("parser_raw",&raw_check);graphics_fd=-1;}}
void xu4_integrity_xml_open(void *ctx,const char *name){if(enabled && ctx && is_graphics(name)){graphics_ctx=ctx;init(&xml_check);}}
void xu4_integrity_xml_read(void *ctx,const void *data,int n){
 if(!enabled || !ctx || ctx!=graphics_ctx)return;
 if(n<0){++xml_check.errors;return;}
 check(&xml_check,data,(unsigned)n);
}
void xu4_integrity_xml_close(void *ctx){if(enabled && ctx && ctx==graphics_ctx){report("parser_xml_input",&xml_check);graphics_ctx=NULL;}}
void xu4_integrity_finish(void){enabled=0;graphics_fd=-1;graphics_ctx=NULL;}


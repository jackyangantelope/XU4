/* Core0-only diagnostic scope. No allocation, caching or synchronous logging. */
#include "xu4_read_diag.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
struct Bucket {uint32_t calls;uint64_t requested,actual,us;uint32_t max_us;};
struct Event {
 uint64_t start,elapsed,read_us,open_us,seek_us,requested,actual;
 uint32_t calls,errors,shorts,zeros,min_req,max_req,max_us,max_req_at,max_actual_at,max_pos_at;
 uint32_t opens,open_errors,open_max_us,file_size,seeks,seek_errors,seek_max_us,backward;
 int id,rc;
 struct Bucket bins[5];
};
static struct Event current,queue[4];
static unsigned active,head,count,completed,dropped,phase;
int xu4_read_active(void){return active;}
void xu4_read_begin(int id,const char *path){
 const char *base=path?strrchr(path,'/'):NULL;base=base?base+1:path;
 active=0;
 if(id!=2 || !base || strcmp(base,"towne_2.it"))return;
 memset(&current,0,sizeof current);current.id=id;current.start=time_us_64();active=1;
}
void xu4_read_record(unsigned req,unsigned got,unsigned error,uint64_t us,unsigned pos){
 if(!active)return;
 unsigned b=req==0?0:req<=64?1:req<=512?2:req<=4096?3:4;
 struct Bucket *bucket=&current.bins[b];
 ++bucket->calls;bucket->requested+=req;bucket->actual+=got;bucket->us+=us;
 if(us>bucket->max_us)bucket->max_us=(uint32_t)us;
 if(!current.calls || req<current.min_req)current.min_req=req;
 if(req>current.max_req)current.max_req=req;
 if(!current.calls || us>current.max_us){current.max_us=(uint32_t)us;current.max_req_at=req;current.max_actual_at=got;current.max_pos_at=pos;}
 ++current.calls;current.requested+=req;current.actual+=got;current.read_us+=us;
 current.errors+=error!=0;current.shorts+=got<req;current.zeros+=got==0;
}
void xu4_read_open(unsigned error,uint64_t us,unsigned size){
 if(!active)return;
 ++current.opens;current.open_errors+=error!=0;current.open_us+=us;
 if(us>current.open_max_us)current.open_max_us=(uint32_t)us;
 if(!error)current.file_size=size;
}
void xu4_read_seek(unsigned error,uint64_t us,unsigned from,unsigned to){
 if(!active)return;
 ++current.seeks;current.seek_errors+=error!=0;current.seek_us+=us;
 if(us>current.seek_max_us)current.seek_max_us=(uint32_t)us;
 current.backward+=to<from;
}
void xu4_read_end(int rc){
 if(!active)return;
 current.elapsed=time_us_64()-current.start;current.rc=rc;active=0;++completed;
 if(count==4){++dropped;return;}
 queue[(head+count)%4]=current;++count;
}
void xu4_read_report(uint64_t now){
 if(!count && !completed && !dropped)return;
 printf("XU4_READ_WIN ms=%lu completed=%u pending=%u dropped=%u scope=xmp_load_module_all_files layer=FatFS\n",(unsigned long)(now/1000),completed,count,dropped);
 completed=dropped=0;
 if(!count)return;
 const struct Event *e=&queue[head];
 if(phase==0) printf("XU4_READ_LOAD start_us=%llu id=%d file=towne_2.it rc=%d elapsed_us=%llu calls=%lu requested=%llu actual=%llu read_us=%llu min_req=%lu max_req=%lu max_us=%lu max_req_at=%lu max_actual_at=%lu max_pos_at=%lu errors=%lu short_reads=%lu zero_reads=%lu\n",(unsigned long long)e->start,e->id,e->rc,(unsigned long long)e->elapsed,(unsigned long)e->calls,(unsigned long long)e->requested,(unsigned long long)e->actual,(unsigned long long)e->read_us,(unsigned long)e->min_req,(unsigned long)e->max_req,(unsigned long)e->max_us,(unsigned long)e->max_req_at,(unsigned long)e->max_actual_at,(unsigned long)e->max_pos_at,(unsigned long)e->errors,(unsigned long)e->shorts,(unsigned long)e->zeros);
 if(phase==1) printf("XU4_READ_IO start_us=%llu opens=%lu open_errors=%lu open_us=%llu open_max_us=%lu file_size=%lu seeks=%lu backward=%lu seek_errors=%lu seek_us=%llu seek_max_us=%lu\n",(unsigned long long)e->start,(unsigned long)e->opens,(unsigned long)e->open_errors,(unsigned long long)e->open_us,(unsigned long)e->open_max_us,(unsigned long)e->file_size,(unsigned long)e->seeks,(unsigned long)e->backward,(unsigned long)e->seek_errors,(unsigned long long)e->seek_us,(unsigned long)e->seek_max_us);
 if(phase==2){
 printf("XU4_READ_BINS start_us=%llu ranges=0,1-64,65-512,513-4096,4097+ tuple=calls:requested:actual:us:max_us",(unsigned long long)e->start);
 for(unsigned i=0;i<5;++i){const struct Bucket *b=&e->bins[i];printf(" b%u=%lu:%llu:%llu:%llu:%lu",i,(unsigned long)b->calls,(unsigned long long)b->requested,(unsigned long long)b->actual,(unsigned long long)b->us,(unsigned long)b->max_us);}
 printf("\n");}
 if(++phase==3){phase=0;head=(head+1)%4;--count;}
}


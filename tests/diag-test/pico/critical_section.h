#pragma once
#include <cassert>
struct critical_section_t { bool held=false; };
inline void critical_section_init(critical_section_t*){}
inline void critical_section_enter_blocking(critical_section_t *p){assert(!p->held);p->held=true;}
inline void critical_section_exit(critical_section_t *p){assert(p->held);p->held=false;}

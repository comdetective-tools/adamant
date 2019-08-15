#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <dlfcn.h>
#include <cstdint>
#include <cstring>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include <adm.h>
#include <adm_config.h>
#include <adm_common.h>
#include <adm_memory.h>
#include <adm_database.h>

using namespace adamant;

ADM_VISIBILITY void* (*adamant::malloc_ptr)(size_t) = nullptr;
ADM_VISIBILITY void  (*adamant::free_ptr)(void*) = nullptr;
ADM_VISIBILITY void* (*adamant::realloc_ptr)(void *,size_t) = nullptr;
ADM_VISIBILITY void* (*adamant::calloc_ptr)(size_t,size_t) = nullptr;
ADM_VISIBILITY void* (*adamant::valloc_ptr)(size_t) = nullptr;
ADM_VISIBILITY void* (*adamant::pvalloc_ptr)(size_t) = nullptr;
ADM_VISIBILITY void* (*adamant::memalign_ptr)(size_t,size_t) = nullptr;
ADM_VISIBILITY void* (*adamant::aligned_alloc_ptr)(size_t,size_t) = nullptr;
ADM_VISIBILITY int   (*adamant::posix_memalign_ptr)(void**,size_t,size_t) = nullptr;
ADM_VISIBILITY void* (*adamant::mmap_ptr)(void*,size_t,int,int,int,off_t) = nullptr;
ADM_VISIBILITY void* (*adamant::mmap64_ptr)(void*,size_t,int,int,int,off64_t) = nullptr;
ADM_VISIBILITY int   (*adamant::munmap_ptr)(void*,size_t) = nullptr;

pool_t<adamant::stack_t, ADM_META_STACK_BLOCKSIZE>* stacks;

static uint8_t static_buffer[ADM_MEM_STATIC_BUFFER];
static uint8_t* static_buffer_ptr=static_buffer;
uint8_t init_posix = 0;

static int string_ends_with(const char *str, const char *suf) {
    size_t str_len = strlen(str);
    size_t suf_len = strlen(suf);

    return !strncmp(str + str_len - suf_len, suf, suf_len);
}

inline
void get_stack(adamant::stack_t& frames)
{
  unw_cursor_t cursor; unw_context_t uc;
  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);

  uint32_t func = 0;
  for(uint8_t f=0; f < ADM_META_STACK_DEPTH && unw_step(&cursor) > 0 && func < ADM_META_STACK_NAMES; ++f) {
    unw_get_reg(&cursor, UNW_REG_IP, &frames.ip[f]);
    unw_word_t offp;
    char sym[256];
    unw_get_proc_name(&cursor, sym, ADM_META_STACK_NAMES-func, &offp);

    if(strlen(sym) >= 8 && strncmp(sym, "OnSample", 8) == 0)
	//fprintf(stderr, "OnSample is detected\n");
	continue;
      if(strlen(sym) >= 18 && strncmp(sym, "perf_event_handler", 18) == 0)
	//fprintf(stderr, "perf_event_handler is detected\n");
	continue;
      if(strlen(sym) >= 22 && strncmp(sym, "monitor_signal_handler", 22) == 0)
	//fprintf(stderr, "monitor_signal_handler is detected\n");
	continue;
      if(strlen(sym) >= 6 && strncmp(sym, "killpg", 6) == 0)
	//fprintf(stderr, "killpg is detected\n");
	continue;
      if(strlen(sym) >= 22 && strncmp(sym, "ComDetectiveWPCallback", 22) == 0)
	//fprintf(stderr, "ComDetectiveWPCallback is detected\n");
	continue;
      if(strlen(sym) >= 12 && strncmp(sym, "OnWatchPoint", 12) == 0)
	//fprintf(stderr, "OnWatchPoint is detected\n");
	continue;
      if(string_ends_with(sym, "by_object_id"))
	continue;

    strncpy(&frames.function[func], sym, strlen(sym));
    func += strlen(&frames.function[func])+1;
  }
}

//static inline
void pointers_init()
{
  fprintf(stderr, "calloc is initialized\n");
  /*malloc_ptr = reinterpret_cast<void*(*)(size_t)>(dlsym(RTLD_NEXT, "malloc"));
  free_ptr = reinterpret_cast<void(*)(void*)>(dlsym(RTLD_NEXT, "free"));
  realloc_ptr = reinterpret_cast<void*(*)(void*,size_t)>(dlsym(RTLD_NEXT, "realloc"));*/
  //calloc_ptr = reinterpret_cast<void*(*)(size_t,size_t)>(dlsym(RTLD_NEXT, "calloc"));
  fprintf(stderr, "calloc is initialized 2\n");
  /*valloc_ptr = reinterpret_cast<void*(*)(size_t)>(dlsym(RTLD_NEXT, "valloc"));
  pvalloc_ptr = reinterpret_cast<void*(*)(size_t)>(dlsym(RTLD_NEXT, "pvalloc"));
  memalign_ptr = reinterpret_cast<void*(*)(size_t,size_t)>(dlsym(RTLD_NEXT, "memalign"));
  aligned_alloc_ptr = reinterpret_cast<void*(*)(size_t,size_t)>(dlsym(RTLD_NEXT, "aligned_alloc_ptr"));
  posix_memalign_ptr = reinterpret_cast<int(*)(void**,size_t,size_t)>(dlsym(RTLD_NEXT, "posix_memalign"));
  mmap_ptr = reinterpret_cast<void*(*)(void*,size_t,int,int,int,off_t)>(dlsym(RTLD_NEXT, "mmap"));
  mmap64_ptr = reinterpret_cast<void*(*)(void*,size_t,int,int,int,off64_t)>(dlsym(RTLD_NEXT, "mmap64"));
  munmap_ptr = reinterpret_cast<int(*)(void*,size_t)>(dlsym(RTLD_NEXT, "munmap"));*/
}

extern "C" void malloc_adm(void* ptr, size_t size, int object_id)
{
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, object_id, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
  //return ptr;
}

extern "C" void free_adm(void* ptr)
{
  //if(static_cast<uint8_t*>(ptr)<static_buffer || static_cast<uint8_t*>(ptr)>=static_buffer+ADM_MEM_STATIC_BUFFER) {
    //if(!free_ptr) pointers_init();
    //free_ptr(ptr);
    if(ptr && adm_set_tracing(0)) {
      adm_db_update_state(reinterpret_cast<uint64_t>(ptr), ADM_STATE_FREE);
      adm_set_tracing(1);
    }
  //}
}

extern "C" void realloc_adm(void* ptr, size_t size, int object_id)
{
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, object_id, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
}

extern "C" void posix_memalign_adm(int ret, void** memptr, size_t alignment, size_t size, int object_id)
{
  if(ret==0 && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(*memptr), size, object_id, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
}

extern "C" void memalign_adm(void * ptr, size_t size, int object_id)
{
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, object_id, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
}

extern "C" void aligned_alloc_adm(void *ptr, size_t size, int object_id)
{
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, object_id, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
}


extern "C" void valloc_adm(void* ptr, size_t size, int object_id)
{
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, object_id, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
}

extern "C" void pvalloc_adm(void* ptr, size_t size, int object_id)
{
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, object_id, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
}


extern "C" void numa_alloc_onnode_adm(void* ptr, size_t size, int object_id)
{
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, object_id, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
}


extern "C" void numa_alloc_interleaved_adm(void* ptr, size_t size, int object_id)
{
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, object_id, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
}

extern "C" void mmap_adm(void* ptr, size_t size, int object_id)
{
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, object_id, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
}

extern "C" void mmap64_adm(void* ptr, size_t size, int object_id)
{
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, object_id, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
}

void adamant::adm_posix_init()
{
  stacks = new pool_t<adamant::stack_t, ADM_META_STACK_BLOCKSIZE>;
}

void adamant::adm_posix_fini()
{
  delete stacks;
}

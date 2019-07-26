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

static pool_t<adamant::stack_t, ADM_META_STACK_BLOCKSIZE>* stacks;

static uint8_t static_buffer[ADM_MEM_STATIC_BUFFER];
static uint8_t* static_buffer_ptr=static_buffer;
uint8_t init_posix = 0;

static inline
void get_stack(adamant::stack_t& frames)
{
  unw_cursor_t cursor; unw_context_t uc;
  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);

  uint32_t func = 0;
  for(uint8_t f=0; f < ADM_META_STACK_DEPTH && unw_step(&cursor) > 0 && func < ADM_META_STACK_NAMES; ++f) {
    unw_get_reg(&cursor, UNW_REG_IP, &frames.ip[f]);
    unw_word_t offp;
    unw_get_proc_name(&cursor, &frames.function[func], ADM_META_STACK_NAMES-func, &offp);
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
  calloc_ptr = reinterpret_cast<void*(*)(size_t,size_t)>(dlsym(RTLD_NEXT, "calloc"));
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

extern "C" void* malloc_adm(void* ptr, size_t size)
{
  /*if(!malloc_ptr) {
    if(init_posix) {
      if(size<ADM_MEM_MIN_ALLOC) size=ADM_MEM_MIN_ALLOC;
      if(static_buffer_ptr+size>static_buffer+ADM_MEM_STATIC_BUFFER) return 0; 
      uint8_t* buffer = static_buffer_ptr;
      static_buffer_ptr+=size;
      return buffer;
    }
    else {
      init_posix=1;
      pointers_init();
    }
  }

  void* ptr = malloc_ptr(size);*/
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
  return ptr;
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

extern "C" void* calloc_adm(size_t nmemb, size_t size)
{
  if(!calloc_ptr) {
    if(init_posix) {
      fprintf(stderr, "calloc 1\n");
      size*=nmemb;
      if(size<ADM_MEM_MIN_ALLOC) size=ADM_MEM_MIN_ALLOC;
      if(static_buffer_ptr+size>static_buffer+ADM_MEM_STATIC_BUFFER) return 0; 
      uint8_t* buffer = static_buffer_ptr;
      static_buffer_ptr+=size;
      memset(buffer,0,size);
      return buffer;
    }
    else {
      fprintf(stderr, "calloc 2\n");
      init_posix=1;
      pointers_init();
    }
  }
  fprintf(stderr, "calloc before\n");
  void* ptr = calloc_ptr(nmemb,size);
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
  fprintf(stderr, "calloc after\n");
  return ptr;
}

extern "C" void* calloc_adm2(size_t nmemb, size_t size)
{
  if(!calloc_ptr) {
    if(init_posix) {
      size*=nmemb;
      if(size<ADM_MEM_MIN_ALLOC) size=ADM_MEM_MIN_ALLOC;
      if(static_buffer_ptr+size>static_buffer+ADM_MEM_STATIC_BUFFER) return 0; 
      uint8_t* buffer = static_buffer_ptr;
      static_buffer_ptr+=size;
      memset(buffer,0,size);
      return buffer;
    }
    else {
      init_posix=1;
      pointers_init();
    }
  }

  void* ptr = calloc_ptr(nmemb,size);
  return ptr;
}

extern "C" void realloc_adm(void *pptr, void *ptr, size_t size)
{
  if(static_cast<uint8_t*>(ptr)<static_buffer || static_cast<uint8_t*>(ptr)>=static_buffer+ADM_MEM_MIN_ALLOC) {
    //pptr = realloc_ptr(ptr,size);
    if(adm_set_tracing(0)) {
      if(ptr) {
        if(size) {
          if(pptr==ptr)
            adm_db_update_size(reinterpret_cast<uint64_t>(ptr), size);
          else if(pptr) {
            adm_db_update_state(reinterpret_cast<uint64_t>(ptr), ADM_STATE_FREE);
            adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(pptr), size, ADM_STATE_ALLOC);
            if(obj) {
              if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
                get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
            }
          }
        }
        else
          adm_db_update_state(reinterpret_cast<uint64_t>(ptr), ADM_STATE_FREE);
      }
      else if(pptr) {
        adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(pptr), size, ADM_STATE_ALLOC);
        if(obj) {
          if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
            get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
        }
      }
      adm_set_tracing(1);
    }
  }
  else {
    pptr = realloc_ptr(0,size);
    if(pptr && adm_set_tracing(0)) {
      adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(pptr), size, ADM_STATE_ALLOC);
      if(obj) {
        if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
          get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
      }
      adm_set_tracing(1);
    }

    if(pptr) {
      if(static_cast<uint8_t*>(ptr)+size>static_buffer+ADM_MEM_STATIC_BUFFER)
        memcpy(pptr, ptr, static_buffer+ADM_MEM_STATIC_BUFFER-static_cast<uint8_t*>(ptr));
      else
        memcpy(pptr, ptr, size);
    }
  }

  //return pptr;
}

/*
extern "C" void* realloc_adm(void *ptr, size_t size)
{
  if(!realloc_ptr) {
    if(ptr && size==0)
      return 0;

    if(init_posix) {
      size_t osize = size;
      if(size<ADM_MEM_MIN_ALLOC) size=ADM_MEM_MIN_ALLOC;
      if(static_buffer_ptr+size>static_buffer+ADM_MEM_STATIC_BUFFER) return 0; 
      uint8_t* buffer = static_buffer_ptr;
      static_buffer_ptr+=size;
      if(ptr) memcpy(buffer, ptr, osize);
      return buffer;
    }
    else {
      init_posix=1;
      pointers_init();
    }
  }

  if(!realloc_ptr) pointers_init();
  void* pptr=nullptr;
  if(static_cast<uint8_t*>(ptr)<static_buffer || static_cast<uint8_t*>(ptr)>=static_buffer+ADM_MEM_MIN_ALLOC) {
    pptr = realloc_ptr(ptr,size);
    if(adm_set_tracing(0)) {
      if(ptr) {
        if(size) {
          if(pptr==ptr)
            adm_db_update_size(reinterpret_cast<uint64_t>(ptr), size);
          else if(pptr) {
            adm_db_update_state(reinterpret_cast<uint64_t>(ptr), ADM_STATE_FREE);
            adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(pptr), size, ADM_STATE_ALLOC);
            if(obj) {
              if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
                get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
            }
          }
        }
        else
          adm_db_update_state(reinterpret_cast<uint64_t>(ptr), ADM_STATE_FREE);
      }
      else if(pptr) {
        adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(pptr), size, ADM_STATE_ALLOC);
        if(obj) {
          if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
            get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
        }
      }
      adm_set_tracing(1);
    }
  }
  else {
    pptr = realloc_ptr(0,size);
    if(pptr && adm_set_tracing(0)) {
      adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(pptr), size, ADM_STATE_ALLOC);
      if(obj) {
        if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
          get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
      }
      adm_set_tracing(1);
    }

    if(pptr) {
      if(static_cast<uint8_t*>(ptr)+size>static_buffer+ADM_MEM_STATIC_BUFFER)
        memcpy(pptr, ptr, static_buffer+ADM_MEM_STATIC_BUFFER-static_cast<uint8_t*>(ptr));
      else
        memcpy(pptr, ptr, size);
    }
  }

  return pptr;
}*/

extern "C" void posix_memalign_adm(int ret, void** memptr, size_t alignment, size_t size)
{
  if(ret==0 && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(*memptr), size, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
}

/*
extern "C" int posix_memalign_adm(void** memptr, size_t alignment, size_t size)
{
  if(!posix_memalign_ptr) pointers_init();
  int ret = posix_memalign_ptr(memptr, alignment, size);
  if(ret==0 && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(*memptr), size, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
    
  return ret;
}*/

extern "C" void memalign_adm(void * ptr, size_t size)
{
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
}

/*
extern "C" void* memalign_adm(size_t alignment, size_t size)
{
  if(!memalign_ptr) pointers_init();
  void* ptr = memalign_ptr(alignment, size);
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
  return ptr;
}*/

extern "C" void aligned_alloc_adm(void *ptr, size_t size)
{
  if(ptr && adm_set_tracing(0)) {
    //printf("aligned_alloc: line 4\n");
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, ADM_STATE_ALLOC);
    //printf("aligned_alloc: line 5\n");
    if(obj) {
      //printf("aligned_alloc: line 6\n");
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        //printf("aligned_alloc: line 7\n");
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
        //printf("aligned_alloc: line 8\n");
    }
    //printf("aligned_alloc: line 9\n");
    adm_set_tracing(1);
    //printf("aligned_alloc line 10\n");
  }
  //return ptr;
}

/*
extern "C" void* aligned_alloc_adm(size_t alignment, size_t size)
{
  if(!aligned_alloc_ptr) pointers_init();
  void* ptr = aligned_alloc_ptr(alignment, size);
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
  return ptr;
}*/

extern "C" void valloc_adm(void* ptr, size_t size)
{
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
  //return ptr;
}

/*
extern "C" void* valloc_adm(size_t size)
{
  if(!valloc_ptr) pointers_init();
  void* ptr = valloc_ptr(size);
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
  return ptr;
}*/

extern "C" void pvalloc_adm(void* ptr, size_t size)
{
  //if(!pvalloc_ptr) pointers_init();
  //void* ptr = pvalloc_ptr(size);
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
  //return ptr;
}

/*
extern "C" void* pvalloc_adm(size_t size)
{
  if(!pvalloc_ptr) pointers_init();
  void* ptr = pvalloc_ptr(size);
  if(ptr && adm_set_tracing(0)) {
    adm_object_t* obj = adm_db_insert(reinterpret_cast<uint64_t>(ptr), size, ADM_STATE_ALLOC);
    if(obj) {
      if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    }
    adm_set_tracing(1);
  }
  return ptr;
}*/

void adamant::adm_posix_init()
{
  stacks = new pool_t<adamant::stack_t, ADM_META_STACK_BLOCKSIZE>;
}

void adamant::adm_posix_fini()
{
  delete stacks;
}

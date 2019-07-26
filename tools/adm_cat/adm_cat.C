#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstring>
#include <zlib.h>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include <adm_database.h>

using namespace adamant;

#define BUFFER_SIZE 16384

static unsigned char in_buffer[BUFFER_SIZE];
static unsigned char out_buffer[BUFFER_SIZE];

enum catstate_t {ADDRESS_ST=0, SIZE_ST=1, EVENTS_ST=2, VAR_ST=3, FILE_ST=4, BIN_ST=5, STACK_ST=6};
static catstate_t state = ADDRESS_ST;

inline unsigned int print_buffer(const unsigned char* out_buffer, const unsigned int ready)
{
  unsigned int used = 0;
  size_t len = 0;
  while(used<ready) {
    unsigned int left = ready-used;
    switch(state) {
    case ADDRESS_ST:
      if(left<sizeof(uint64_t)) return used;
      std::cout << std::hex << *reinterpret_cast<const uint64_t*>(out_buffer+used) << std::dec;
      used += sizeof(uint64_t);
      state=SIZE_ST;
      break;
    case    SIZE_ST:
      if(left<sizeof(uint64_t)+sizeof(state_t)) return used;
      std::cout << " " << *reinterpret_cast<const uint64_t*>(out_buffer+used);
      used += sizeof(uint64_t);
      std::cout << " " << *reinterpret_cast<const state_t*>(out_buffer+used) << std::endl;
      used += sizeof(state_t);
      state=EVENTS_ST;
      break;
    case  EVENTS_ST:
      if(left<sizeof(uint64_t)*ADM_EVENTS) return used;
      for(unsigned char e=0; e<ADM_EVENTS; ++e) 
        std::cout << " " << reinterpret_cast<const uint64_t*>(out_buffer+used)[e];
      std::cout << std::endl;
      used += sizeof(uint64_t)*ADM_EVENTS;
      state = VAR_ST;
      break;
    case     VAR_ST:
    case    FILE_ST:
    case     BIN_ST:
      len = strnlen(reinterpret_cast<const char*>(out_buffer+used), left)+1;
      if(len>left) return used;
      std::cout << out_buffer+used << std::endl;
      used += len;
      state = state==VAR_ST ? FILE_ST : (state==FILE_ST ? BIN_ST : STACK_ST);
      break;
    case   STACK_ST:
    default:
      len = strnlen(reinterpret_cast<const char*>(out_buffer+used), left)+1;
      if(len+sizeof(unw_word_t)>left) return used;
      const unw_word_t* ip = reinterpret_cast<const unw_word_t*>(out_buffer+used+len);
      if(len==1 && *ip==0) {
        state = ADDRESS_ST;
        std::cout << std::endl;
      }
      else
        std::cout << " " << out_buffer+used << ":" << std::hex << *ip << std::dec;
      used += len+sizeof(unw_word_t);
    }
  }

  return used;
}


int main(int argc, char* argv[])
{
  if(argc != 2) {
    std::cout << "adm_cat file" << std::endl;
    return 0;
  }
  std::ifstream source(argv[1]);
  if(!source) {
    std::cerr << "error opening file" << std::endl;
    return 0;
  }
  source.getline(reinterpret_cast<char*>(&in_buffer[0]), BUFFER_SIZE);
  std::cout << in_buffer << std::endl;
  source.getline(reinterpret_cast<char*>(&in_buffer[0]), BUFFER_SIZE);
  std::cout << in_buffer << std::endl;

  z_stream strm;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  if(inflateInit(&strm) != Z_OK) {
    std::cerr << "error initializing zlib" << std::endl;;
    return 0;
  }
  strm.avail_out = BUFFER_SIZE;
  strm.next_out = out_buffer;

  unsigned int ready = 0;
  do {
    source.read(reinterpret_cast<char*>(&in_buffer[0]), BUFFER_SIZE);
    strm.avail_in = source.gcount();
    strm.next_in = in_buffer;

    if(strm.avail_in == 0)
      break;

    do {
        switch(inflate(&strm, Z_NO_FLUSH)) {
        case Z_NEED_DICT:
        case Z_DATA_ERROR:
          std::cerr << "invalid or incomplete data" << std::endl;
          inflateEnd(&strm);
          return 0;
        case Z_MEM_ERROR:
          std::cerr << "zlib out of memory" << std::endl;
          inflateEnd(&strm);
          return 0;
        default: ;
        }

        ready = BUFFER_SIZE - strm.avail_out;
        unsigned int used = print_buffer(out_buffer, ready);
        ready -= used;
        if(ready) {
          memmove(out_buffer, out_buffer+used, ready);
          strm.next_out = out_buffer+ready;
          strm.avail_out = BUFFER_SIZE-ready;
        }
        else {
          strm.next_out = out_buffer;
          strm.avail_out = BUFFER_SIZE;
        }

    }while(strm.avail_in);

  }while(true);

  inflateEnd(&strm);

  return 0;
}

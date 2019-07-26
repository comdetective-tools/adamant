#ifndef __ADAMANT_HASH
#define __ADAMANT_HASH

#include <stdint.h>
#include <string.h>

namespace adamant
{

class hash_t
{
  public:

    hash_t* next;
    const char* value;
};

template <uint8_t lsize>
class table_t
{
  private:

    hash_t* map[1<<lsize];

    uint8_t hash(const char* c) const noexcept
    {
      uint32_t h = 5381;
      for(; *c!='\0'; ++c) h+=(h<<5)+static_cast<unsigned char>(*c);
      return static_cast<uint8_t>(h>>(32-lsize));
    }

  public:

    table_t()
    {
      memset(map, 0, sizeof(map));
    }

    void insert(hash_t* entry) noexcept
    {
      uint8_t h = hash(entry->value);
      entry->next = map[h];
      map[h] = entry;
    }
    
    const char* find(const char* value) const noexcept
    {
      uint8_t h = hash(value);
      hash_t* entry = map[h];
      while(entry && strcmp(entry->value, value))
        entry=entry->next;
      return entry? entry->value : nullptr;
    }

};

}

#endif

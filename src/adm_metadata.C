#include <cstdint>
#include <iostream>

#include <adm.h>
#include <adm_config.h>
#include <adm_common.h>
#include <adm_database.h>

using namespace adamant;

static void adm_meta_process_null(void* meta, const adm_event_t& event) {}

static void adm_meta_process_base(void* m, const adm_event_t& event) noexcept
{
  uint64_t* events = static_cast<uint64_t*>(m);
  uint8_t source = 0;
  if(event.data_src&PERF_MEM_OP_STORE)
    source+=ADM_L1_ST;
  else if((event.data_src&PERF_MEM_OP_LOAD)==0)
    goto unknown_event;

  if(event.data_src&(PERF_MEM_LVL_HIT<<PERF_MEM_LVL_SHIFT)) {
    if(event.data_src&(PERF_MEM_LVL_L2<<PERF_MEM_LVL_SHIFT))
      ++source;
    else if(event.data_src&(PERF_MEM_LVL_L3<<PERF_MEM_LVL_SHIFT))
      source+=2;
    else if(event.data_src&(PERF_MEM_LVL_LOC_RAM<<PERF_MEM_LVL_SHIFT))
      source+=3;
    else if((event.data_src&(PERF_MEM_LVL_L1<<PERF_MEM_LVL_SHIFT))==0)
      goto unknown_event;
  }
  else
    goto unknown_event;
  ++events[source];
  unknown_event:;
  ++events[ADM_EV_COUNT];
}

static void adm_meta_print_base(const void* m) noexcept
{
  const uint64_t* events = static_cast<const uint64_t*>(m);
  adm_out(events, sizeof(uint64_t)*ADM_EVENTS);
}

static void adm_meta_print_string(const void* m) noexcept
{
  if(m!=nullptr)
    adm_out(m, strlen(static_cast<const char*>(m)));
  adm_out("\0", 1);
}

static void adm_meta_print_stack(const void* m) noexcept
{
  const adamant::stack_t* stack = static_cast<const adamant::stack_t*>(m);
  if(m!=nullptr) {
    uint32_t func = 0;
    for(uint8_t f=0; f < ADM_META_STACK_DEPTH && stack->ip[f]; ++f) {
      size_t len = strlen(stack->function+func)+1;
      adm_out(stack->function+func, len);
      adm_out((stack->ip+f), sizeof(stack->ip[f]));
      func += len;
    }
  }
  adm_out("\0", 1);
  for(size_t c=0; c < sizeof(stack->ip[0]); ++c)
    adm_out("\0", 1);
}

static void (*printers[ADM_MAX_META_TYPES+1])(const void*) =
  {adm_meta_print_string, adm_meta_print_string, adm_meta_print_string, adm_meta_print_stack, adm_meta_print_base};
static void (*processors[ADM_MAX_META_TYPES+1])(void* meta, const adm_event_t& event) =
  {adm_meta_process_null, adm_meta_process_null, adm_meta_process_null, adm_meta_process_null, adm_meta_process_base};

ADM_VISIBILITY
void adm_meta_t::print() const noexcept
{
  printers[ADM_MAX_META_TYPES](events);
  for(uint8_t m=0; m<ADM_MAX_META_TYPES; ++m)
    printers[m](meta[m]);
}

ADM_VISIBILITY
bool adm_meta_t::has_events() const noexcept
{
  for(auto e = 1; e < ADM_EVENTS; ++e)
    if(events[e]) return true;
  return false;
}

ADM_VISIBILITY
void adm_meta_t::process(const adm_event_t& event) noexcept
{
  processors[ADM_MAX_META_TYPES](events, event);
  for(uint8_t m=0; m<ADM_MAX_META_TYPES; ++m)
    processors[m](meta[m], event);
}

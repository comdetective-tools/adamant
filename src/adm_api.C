#include <cstdint>

#include <adm.h>
#include <adm_common.h>
#include <adm_database.h>

using namespace adamant;

static inline
void adm_process_event(const adm_event_t& event) noexcept
{
  adm_object_t* obj = adm_db_find_by_address(event.address);
  if(obj)
    obj->process(event);
  else
    adm_debug("event dropped: address ", event.address, " not found");
}

void adm_event(const adm_event_t* event)
{
  adm_api("receiving event ", event);
  if(adm_set_tracing(0)) {
    adm_process_event(*event);
    adm_set_tracing(1);
  }
  adm_api("event processed");
}

void adm_events_v(const adm_event_t* events, const uint32_t events_num)
{
  adm_api("receiving ", events_num, " events at ", events);
  if(adm_set_tracing(0)) {
    for(uint32_t e=0; e<events_num; ++e)
      adm_process_event(events[e]);
    adm_set_tracing(1);
  }
  adm_api("events processed");
}

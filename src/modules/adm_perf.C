#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <cstdint>
#include <cstdlib>
#include <unistd.h>
#include <sys/syscall.h>
#include <cstring>
#include <cstdio>
#include <cerrno>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <iostream>

#include <adm.h>
#include <adm_config.h>
#include <adm_common.h>

#define TOSTR(name) #name
#define MOD_NAME(name) TOSTR(name)

static struct perf_event_mmap_page* head = nullptr;
static char* buffer = nullptr;
static uint64_t mask = 0;
static int group = 0;

using namespace adamant;

static char counters[ADM_MAX_STRING];

static inline
long int perf_event_open(struct perf_event_attr *a, pid_t p, int c, int g, unsigned long f)
{
  return syscall(SYS_perf_event_open,a,p,c,g,f);
}

struct read_format {
  uint64_t value;
  uint64_t time_enabled;
  uint64_t time_running;
  uint64_t id;
};

static
void read_buffer(int restart)
{
  ioctl(group, PERF_EVENT_IOC_DISABLE, 0);
  while(head->data_tail<head->data_head) {
    adm_event_t* event = reinterpret_cast<adm_event_t*>(buffer+(head->data_tail&mask));
    if(event->hdr.type == PERF_RECORD_SAMPLE && event->address)
      adm_event(event);
    head->data_tail += event->hdr.size;
  }

  if(restart) ioctl(group, PERF_EVENT_IOC_ENABLE, 0);
}

ADM_VISIBILITY
void adm_mod_init()
{
  adm_conf_line("+hw", counters);
  if(counters[0] == '\0') adm_module = MOD_NAME(EVENT);
  else adm_module = counters;

  long ps = sysconf(_SC_PAGESIZE);
  mask = ps-1;

  struct perf_event_attr attr;
  memset(&attr, 0, sizeof(attr));
  attr.type = PERF_TYPE_RAW;
  attr.size = sizeof(attr);
  if(counters[0] == '\0') {
    attr.config = EVENT;
    attr.sample_period = PERIOD;
  }
  else {
    char* period=nullptr;
    attr.config = strtoul(counters, &period, 16);
    char* frequency=nullptr;
    attr.sample_period = strtoul(period, &frequency, 10);
    if(period==frequency) attr.sample_period = PERIOD;
    if(frequency && frequency[0]=='1') attr.freq=1;
  }
  attr.sample_type = PERF_SAMPLE_IP | PERF_SAMPLE_TID | PERF_SAMPLE_TIME |
                     PERF_SAMPLE_ADDR | PERF_SAMPLE_CPU | PERF_SAMPLE_STREAM_ID |
                     PERF_SAMPLE_WEIGHT | PERF_SAMPLE_DATA_SRC;

  attr.disabled = 1;
  attr.exclude_kernel = 1;
  attr.exclude_hv = 1;
  attr.exclude_idle = 1;
  attr.watermark = 1;
  //attr.wakeup_events = ps/sizeof(adm_event_t);
  attr.wakeup_watermark = 100;
  attr.precise_ip = 2;

  group = perf_event_open(&attr, 0, -1, -1, 0);
  if(group<0)
    adm_warning("perf_event_open failed in perf module!\n");
  
  head = static_cast<struct perf_event_mmap_page*>(mmap(NULL, ps<<1, PROT_READ|PROT_WRITE, MAP_SHARED, group, 0));
  buffer = reinterpret_cast<char*>(head)+ps;
  if(head==MAP_FAILED)
    adm_warning("mmap failed in perf module!\n");
  
  struct sigaction sa;
  memset(&sa, 0, sizeof(sa));
  sa.sa_handler = read_buffer;
  sigaction(SIGIO, &sa, nullptr);
  sigset_t sigio;
  sigemptyset(&sigio);
  sigaddset(&sigio,SIGIO);
  sigprocmask(SIG_UNBLOCK, &sigio, NULL);

  fcntl(group, F_SETFL, fcntl(group,F_GETFL,0)|O_ASYNC);
  fcntl(group, F_SETOWN, getpid());

  ioctl(group, PERF_EVENT_IOC_RESET, 0);
  ioctl(group, PERF_EVENT_IOC_ENABLE, 0);

}

ADM_VISIBILITY
void adm_mod_fini()
{
  ioctl(group, PERF_EVENT_IOC_DISABLE, 0);
  read_buffer(0);
  long ps = sysconf(_SC_PAGESIZE);
  munmap(head, ps<<1);
}

#include <sys/types.h>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <zlib.h>

#include <adm_memory.h>
#include <adm_common.h>
#include <adm_database.h>
#include <adm_elf.h>

#define BUFFER_SIZE 16384

using namespace adamant;

extern pool_t<adamant::stack_t, ADM_META_STACK_BLOCKSIZE>* stacks;

extern inline
void get_stack(adamant::stack_t& frames);

ADM_VISIBILITY
unsigned char adamant::adm_tracing;

static std::ofstream* adm_fout = nullptr;
static unsigned char out_buffer[BUFFER_SIZE];
static unsigned char zip_buffer[BUFFER_SIZE];
static z_stream strm;
static unsigned int fill = 0;

ADM_VISIBILITY
const char* adamant::adm_module = "ADAMANT";

int matrix_size;
int core_matrix_size;
extern uint8_t init_posix;

static inline
void adm_io_init() noexcept
{
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  if(deflateInit(&strm, Z_BEST_COMPRESSION) != Z_OK)
    adm_warning("Cannot initialize compression, adamant output will be in raw binary format");
  adm_fout = new std::ofstream(std::to_string(getpid())+".adm", std::ofstream::out | std::ofstream::app | std::ofstream::binary);
  if(!adm_fout->good()) {
    adm_warning("Cannot open output file, output disabled!");
    adm_fout->close();
    adm_fout=nullptr;
  }
  else {
    std::ifstream cmd("/proc/self/cmdline");
    if(cmd) {
      char commandline[1024];
      cmd.read(commandline, 1024);
      unsigned int c = cmd.gcount();
      *adm_fout << commandline;
      unsigned int r = strlen(commandline)+1;
      while(r<c-1) {
        *adm_fout << " " << commandline;
        r += strlen(commandline+r)+1;
      }
      *adm_fout << std::endl;
    }
    else
      adm_warning("Unable to read command line!");
  }
}

/*
static inline
void adm_io_init() noexcept
{
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;
  strm.opaque = Z_NULL;
  if(deflateInit(&strm, Z_BEST_COMPRESSION) != Z_OK)
    adm_warning("Cannot initialize compression, adamant output will be in raw binary format");
  adm_fout = new std::ofstream(std::to_string(getpid())+".adm", std::ofstream::out | std::ofstream::app | std::ofstream::binary);
  if(!adm_fout->good()) {
    adm_warning("Cannot open output file, output disabled!");
    adm_fout->close();
    adm_fout=nullptr;
  }
  else {
    std::ifstream cmd("/proc/self/cmdline");
    if(cmd) {
      char commandline[1024];
      cmd.read(commandline, 1024);
      unsigned int c = cmd.gcount();
      *adm_fout << commandline;
      unsigned int r = strlen(commandline)+1;
      while(r<c-1) {
        *adm_fout << " " << commandline;
        r += strlen(commandline+r)+1;
      }
      *adm_fout << std::endl;
    }
    else
      adm_warning("Unable to read command line!");
  }
}*/

static inline
void compress(unsigned char flush)
{
  strm.avail_in = fill;
  strm.next_in = out_buffer;

  do {
    strm.avail_out = BUFFER_SIZE;
    strm.next_out = zip_buffer;
    deflate(&strm, flush);
    //try TODO
    adm_fout->write(reinterpret_cast<const char*>(zip_buffer), BUFFER_SIZE-strm.avail_out);
  }while(strm.avail_in);
}

static inline
void adm_io_fini() noexcept
{
  if(fill)
    compress(Z_FINISH);
  deflateEnd(&strm);
  adm_fout->close();
}

void __attribute__((weak)) adm_mod_init() {};
void __attribute__((weak)) adm_mod_fini() {};

extern int var_id_count;

extern "C"
//__attribute__((constructor))
void adm_initialize()
{
  init_posix = 1;
  var_id_count = 2;
  pointers_init();
  //fprintf(stderr, "adm is initialized\n");
  //adm_io_init();
  //fprintf(stderr, "after adm_io_init\n");
  adm_meta_init();
  //fprintf(stderr, "after adm_meta_init\n");
  adm_db_init();
  //fprintf(stderr, "after adm_db_init\n");
  adm_elf_init();
  //fprintf(stderr, "after adm_elf_init\n");
  adm_posix_init();
  //fprintf(stderr, "after adm_posix_init\n");
  adm_mod_init();
  //fprintf(stderr, "after adm_mod_init\n");
  adm_set_tracing(1);
}

extern "C"
//__attribute__((destructor))
void adm_finalize(int flag, char output_directory[], const char * executable_name, int pid)
{
  adm_set_tracing(0);

  //*adm_fout << adm_module << std::endl;
  //if(adm_fout!=nullptr) adm_db_print();
  //fprintf(stderr, "before adm_db_print\n");
  if(flag)
  	adm_db_print(output_directory, executable_name, pid);
  //fprintf(stderr, "after adm_db_print\n");

  adm_mod_fini();
  adm_posix_fini();
  adm_elf_fini();
  adm_db_fini();
  adm_meta_fini();
  //adm_io_fini();
}


extern "C"
void matrix_size_set(int size)
{
  matrix_size = size;
}

extern "C"
void core_matrix_size_set(int size)
{
  core_matrix_size = size;
}

extern "C"
char * adm_get_var_name(uint64_t address)
{
  return adm_db_get_var_name(address);
}

extern "C"
//__attribute__((destructor))
void inc_false_matrix(uint64_t address1, uint64_t address2, int a, int b, double inc)
{
  adm_object_t* obj1 = adm_db_find_by_address(address1);
  adm_object_t* obj2 = adm_db_find_by_address(address2);
  if(obj1 && obj2 && (obj1->get_object_id() == obj2->get_object_id())) {
  	obj1->inc_fs_matrix(a, b, inc);
  }
}

// before
extern "C"
//__attribute__((destructor))
void inc_false_matrix_by_object_id(int object_id, int a, int b, double inc)
{
  //fprintf(stderr, "increment per object happens for false sharing\n");
  adm_object_t* obj = adm_db_find_by_object_id(object_id); 

  if(obj) {
  	obj->inc_fs_matrix(a, b, inc);
  } else {
	obj = adm_db_insert_by_object_id(object_id, ADM_STATE_ALLOC);
	if(obj) {
		obj->inc_fs_matrix(a, b, inc);
      		if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        		get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    	}
  }
}
// after

extern "C"
//__attribute__((destructor))
void inc_false_core_matrix(uint64_t address1, uint64_t address2, int a, int b, double inc)
{
  adm_object_t* obj1 = adm_db_find_by_address(address1);
  adm_object_t* obj2 = adm_db_find_by_address(address2);
  if(obj1 && obj2 && (obj1->get_object_id() == obj2->get_object_id())) {
	obj1->inc_fs_core_matrix(a, b, inc);
  } 
}

extern "C"
//__attribute__((destructor))
void inc_false_core_matrix_by_object_id(int object_id, int a, int b, double inc)
{
  //fprintf(stderr, "increment per object happens for false sharing\n");
  adm_object_t* obj = adm_db_find_by_object_id(object_id); 

  if(obj) {
  	obj->inc_fs_core_matrix(a, b, inc);
  } else {
	obj = adm_db_insert_by_object_id(object_id, ADM_STATE_ALLOC);
	if(obj) {
		obj->inc_fs_core_matrix(a, b, inc);
      		if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        		get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    	}
  }
}

extern "C"
//__attribute__((destructor))
void inc_false_count(uint64_t address1, uint64_t address2, double inc)
{
  //fprintf(stderr, "increment per object happens for false sharing\n");
  adm_object_t* obj1 = adm_db_find_by_address(address1);
  adm_object_t* obj2 = adm_db_find_by_address(address2);

  //fprintf(stderr, "increment per object happens for false sharing on object with ids %d and %d\n", obj1->get_object_id(), obj2->get_object_id());

  if(obj1 && obj2) {
 	int id1 = obj1->get_object_id();
	int id2 = obj2->get_object_id();
        if((id1 == id2) && (id1 > 1)) {
                obj1->inc_fs_count(inc);
        } 
  }
}

extern "C"
//__attribute__((destructor))
void inc_false_count_by_object_id(int object_id, double inc)
{
  //fprintf(stderr, "increment per object happens for false sharing\n");
  adm_object_t* obj = adm_db_find_by_object_id(object_id); 

  if(obj) {
  	obj->inc_fs_count(inc);
  } else {
	obj = adm_db_insert_by_object_id(object_id, ADM_STATE_ALLOC);
	if(obj) {
		obj->inc_fs_count(inc);
      		if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        		get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    	}
  }
}

extern "C"
int get_object_id_by_address(uint64_t address)
{
  adm_object_t* obj = adm_db_find_by_address(address);

  if(obj) {
        return obj->get_object_id();
  }
  return -1;
}

extern "C"
//__attribute__((destructor))
void inc_false_core_count(uint64_t address1, uint64_t address2, double inc)
{

  adm_object_t* obj1 = adm_db_find_by_address(address1);
  adm_object_t* obj2 = adm_db_find_by_address(address2);

  if(obj1 && obj2) {
	int id1 = obj1->get_object_id();
	int id2 = obj2->get_object_id();
        if((id1 == id2) && (id1 > 1)) {
                obj1->inc_fs_core_count(inc);
        } 
  }
}

extern "C"
//__attribute__((destructor))
void inc_false_core_count_by_object_id(int object_id, double inc)
{
  //fprintf(stderr, "increment per object happens for false sharing\n");
  adm_object_t* obj = adm_db_find_by_object_id(object_id); 

  if(obj) {
  	obj->inc_fs_core_count(inc);
  } else {
	obj = adm_db_insert_by_object_id(object_id, ADM_STATE_ALLOC);
	if(obj) {
		obj->inc_fs_core_count(inc);
      		if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        		get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    	}
  }
}

extern "C"
//__attribute__((destructor))
void inc_true_matrix(uint64_t address, int a, int b, double inc)
{
  //fprintf(stderr, "increment per object happens for true sharing\n");
  adm_object_t* obj = adm_db_find_by_address(address);
  if(obj) {
    obj->inc_ts_matrix(a, b, inc);
  }
}

extern "C"
//__attribute__((destructor))
void inc_true_matrix_by_object_id(int object_id, int a, int b, double inc)
{
  //fprintf(stderr, "increment per object happens for false sharing\n");
  adm_object_t* obj = adm_db_find_by_object_id(object_id); 

  if(obj) {
  	obj->inc_ts_matrix(a, b, inc);
  } else {
	obj = adm_db_insert_by_object_id(object_id, ADM_STATE_ALLOC);
	if(obj) {
		obj->inc_ts_matrix(a, b, inc);
      		if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        		get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    	}
  }
}

extern "C"
//__attribute__((destructor))
void inc_true_core_matrix(uint64_t address, int a, int b, double inc)
{
  adm_object_t* obj = adm_db_find_by_address(address);
  if(obj) {
    obj->inc_ts_core_matrix(a, b, inc);
  }
}

extern "C"
//__attribute__((destructor))
void inc_true_core_matrix_by_object_id(int object_id, int a, int b, double inc)
{
  //fprintf(stderr, "increment per object happens for false sharing\n");
  adm_object_t* obj = adm_db_find_by_object_id(object_id); 

  if(obj) {
  	obj->inc_ts_core_matrix(a, b, inc);
  } else {
	obj = adm_db_insert_by_object_id(object_id, ADM_STATE_ALLOC);
	if(obj) {
		obj->inc_ts_core_matrix(a, b, inc);
      		if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        		get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    	}
  }
}

extern "C"
//__attribute__((destructor))
void inc_true_count(uint64_t address, double inc)
{
  adm_object_t* obj = adm_db_find_by_address(address);
  if(obj && (obj->get_object_id() > 1)) {
    obj->inc_ts_count(inc);
  }
}

extern "C"
//__attribute__((destructor))
void inc_true_count_by_object_id(int object_id, double inc)
{
  //fprintf(stderr, "increment per object happens for false sharing\n");
  adm_object_t* obj = adm_db_find_by_object_id(object_id); 

  if(obj) {
  	obj->inc_ts_count(inc);
  } else {
	obj = adm_db_insert_by_object_id(object_id, ADM_STATE_ALLOC);
	if(obj) {
		obj->inc_ts_count(inc);
      		if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        		get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    	}
  }
}

extern "C"
//__attribute__((destructor))
void inc_true_core_count(uint64_t address, double inc)
{
  adm_object_t* obj = adm_db_find_by_address(address);
  if(obj && (obj->get_object_id() > 1)) {
    obj->inc_ts_core_count(inc);
  }
}

extern "C"
//__attribute__((destructor))
void inc_true_core_count_by_object_id(int object_id, double inc)
{
  //fprintf(stderr, "increment per object happens for false sharing\n");
  adm_object_t* obj = adm_db_find_by_object_id(object_id); 

  if(obj) {
  	obj->inc_ts_core_count(inc);
  } else {
	obj = adm_db_insert_by_object_id(object_id, ADM_STATE_ALLOC);
	if(obj) {
		obj->inc_ts_core_count(inc);
      		if((obj->meta.meta[ADM_META_STACK_TYPE] = stacks->malloc()))
        		get_stack(*static_cast<adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]));
    	}
  }
}

ADM_VISIBILITY
void adamant::adm_out(const void* buffer, const unsigned int size)
{
  unsigned int done = 0;
  while(done<size) {
    unsigned int ready = std::min(size-done,BUFFER_SIZE-fill);
    memcpy(out_buffer+fill, static_cast<const char*>(buffer)+done, ready);
    done += ready; fill+=ready;

    if(fill==BUFFER_SIZE) {
      compress(Z_NO_FLUSH);
      fill=0;
    }
  }
}

ADM_VISIBILITY
std::streamoff adamant::adm_conf_line(const char* var, char* line, std::streamoff offset)
{
  char* name = getenv("ADM_CONF");
  if(name!=nullptr) {
    std::ifstream conf(name);
    if(!conf.is_open()) adm_warning("Unable to open configuration file ", name);
    else {
      if(offset) conf.seekg(offset, std::ios_base::beg);
      while(!conf.eof()) {
        if(conf.peek() == '#')
          conf.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        else {
          conf.getline(line, ADM_MAX_STRING, ' ');
          if(strcmp(var, line)==0) {
            conf.getline(line, ADM_MAX_STRING);
            break;
          }
          else {
            conf.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            line[0] = '\0';
          }
        }
        offset = conf.gcount();
      }
    }
  }
  return offset;
}

ADM_VISIBILITY
bool adamant::adm_conf_string(const char* var, const char* find)
{
  bool found = false;
  char* name = getenv("ADM_CONF");
  if(name!=nullptr) {
    std::ifstream conf(name);
    if(!conf.is_open()) adm_warning("Unable to open configuration file ", name);
    else {
      char line[ADM_MAX_STRING];
      while(!conf.eof() && !found)
        if(conf.peek() == '#')
          conf.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        else {
          conf.getline(line, ADM_MAX_STRING, ' ');
          if(strcmp(var, line)==0) {
            conf.getline(line, ADM_MAX_STRING);
            if(strstr(find, line))
              found = true;
          }
          else
            conf.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        }
    }
  }
  return found;
}
/*
extern "C"
//__attribute__((destructor))
void print_adm_db()
{
  //printf("in print_adm_db\n");
  adm_db_print();
}*/

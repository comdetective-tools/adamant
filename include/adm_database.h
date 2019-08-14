#ifndef __ADAMANT_DATABASE
#define __ADAMANT_DATABASE

#include <cstdint>
#include <cstring>
#define UNW_LOCAL_ONLY
#include <libunwind.h>

#include <adm.h>
#include <adm_config.h>
#include <adm_common.h>
#include <iostream>
#include <iomanip>
#include <pthread.h>

static pthread_mutex_t object_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t ts_count_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t ts_core_count_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t fs_count_lock = PTHREAD_MUTEX_INITIALIZER;

static pthread_mutex_t fs_core_count_lock = PTHREAD_MUTEX_INITIALIZER;

namespace adamant
{

enum meta_t {ADM_META_VAR_TYPE=0, ADM_META_OBJ_TYPE=1,
             ADM_META_BIN_TYPE=2, ADM_META_STACK_TYPE=3, ADM_MAX_META_TYPES=4};

enum state_t {ADM_STATE_STATIC=0, ADM_STATE_ALLOC=1, ADM_STATE_FREE=2, ADM_STATE_DONE=3};

enum events_t {ADM_L1_LD=0, ADM_L2_LD=1, ADM_L3_LD=2, ADM_MM_LD=3,
               ADM_L1_ST=4, ADM_L2_ST=5, ADM_L3_ST=6, ADM_MM_ST=7, ADM_EV_COUNT=8, ADM_EVENTS=9};

class adm_meta_t
{
  public:

    uint64_t events[ADM_EVENTS];
    void* meta[ADM_MAX_META_TYPES];

    adm_meta_t() { memset(this, 0, sizeof(adm_meta_t)); }

    bool has_events() const noexcept;

    void process(const adm_event_t& event) noexcept;

    void print() const noexcept;
};

class stack_t
{
  public:

    stack_t() { memset(function, 0, sizeof(function)); memset(ip, 0, sizeof(ip)); }

    char function[ADM_META_STACK_NAMES];
    unw_word_t ip[ADM_META_STACK_DEPTH];
};

class adm_object_site_t
{
	public:
	int object_id;
	double communication_count;
	adm_object_site_t(): object_id(-1), communication_count(0) {}
};

class adm_object_t
{
    int object_id;
    double false_sharing_matrix[1024][1024];
    double false_sharing_core_matrix[1024][1024];
    double false_sharing_count;
    double false_sharing_core_count;
    double true_sharing_matrix[1024][1024];
    double true_sharing_core_matrix[1024][1024];
    double true_sharing_count;
    double true_sharing_core_count;
    adm_object_t* parent;
    adm_object_t* left;
    adm_object_t* right;

    void zigzig(adm_object_t* granpa) noexcept
    {
      adm_object_t* tmp = parent->right;
      granpa->left=tmp;
      if(tmp) tmp->parent=granpa;

      parent->left=right;
      if(right) right->parent=parent;
    
      tmp = granpa->parent;

      parent->right=granpa;
      granpa->parent=parent;
      right=parent;
      right->parent=this;

      parent=tmp;
      if(tmp) {
        if(tmp->right==granpa) tmp->right=this;
        else tmp->left=this;
      }
    
    }

    void zagzag(adm_object_t* granpa) noexcept
    {
      adm_object_t* tmp = parent->left;
      granpa->right=tmp;
      if(tmp) tmp->parent=granpa;

      parent->right=left;
      if(left) left->parent=parent;
    
      tmp = granpa->parent;

      parent->left=granpa;
      granpa->parent=parent;
      left=parent;
      left->parent=this;

      parent=tmp;
      if(tmp) {
        if(tmp->right==granpa) tmp->right=this;
        else tmp->left=this;
      }
    }

    void zagzig(adm_object_t* granpa) noexcept
    {
      parent->right=left;
      if(left) left->parent=parent;

      granpa->left=right;
      if(right) right->parent=granpa;
    
      adm_object_t* root = granpa->parent;

      left=parent;
      left->parent=this;
      right=granpa;
      right->parent=this;

      parent=root;
      if(root) {
        if(root->right==granpa) root->right=this;
        else root->left=this;
      }
    }

    void zigzag(adm_object_t* granpa) noexcept
    {
      granpa->right=left;
      if(left) left->parent=granpa;

      parent->left=right;
      if(right) right->parent=parent;
    
      adm_object_t* root = granpa->parent;

      left=granpa;
      granpa->parent=this;
      right=parent;
      parent->parent=this;

      parent=root;
      if(root) {
        if(root->right==granpa) root->right=this;
        else root->left=this;
      }
    }

    void zig() noexcept
    {
      parent->left = right;
      if(right) right->parent = parent;
      right = parent;
      right->parent = this;
      parent = nullptr;
    }
    
    void zag() noexcept
    {
      parent->right = left;
      if(left) left->parent = parent;
      left = parent;
      left->parent = this;
      parent = nullptr;
    }

  public:

    adm_meta_t meta;

    adm_object_t(): object_id(0) {}

    int get_object_id() const noexcept { return object_id; };

    void set_object_id(const int a) noexcept { object_id=a; };

    void inc_fs_matrix(int a, int b, double inc) {false_sharing_matrix[a][b] = false_sharing_matrix[a][b] + inc;};

    void inc_fs_core_matrix(int a, int b, double inc) {false_sharing_core_matrix[a][b] = false_sharing_core_matrix[a][b] + inc;};

    void inc_ts_matrix(int a, int b, double inc) {true_sharing_matrix[a][b] = true_sharing_matrix[a][b] + inc;};

    void inc_ts_core_matrix(int a, int b, double inc) {true_sharing_core_matrix[a][b] = true_sharing_core_matrix[a][b] + inc;};

    void inc_fs_count(double inc) {
	pthread_mutex_lock(&fs_count_lock);
	false_sharing_count = false_sharing_count + inc;
	pthread_mutex_unlock(&fs_count_lock);
    };

    void inc_fs_core_count(double inc) {
	pthread_mutex_lock(&fs_core_count_lock);
	false_sharing_core_count = false_sharing_core_count + inc;
	pthread_mutex_unlock(&fs_core_count_lock);
    };

    void inc_ts_count(double inc) {
	pthread_mutex_lock(&ts_count_lock);
	true_sharing_count = true_sharing_count + inc;
	pthread_mutex_unlock(&ts_count_lock);
    };

    void inc_ts_core_count(double inc) {
	pthread_mutex_lock(&ts_core_count_lock);
	true_sharing_core_count = true_sharing_core_count + inc;
	pthread_mutex_unlock(&ts_core_count_lock);
    };

    double get_fs_count() const noexcept { return false_sharing_count; };

    double get_fs_core_count() const noexcept { return false_sharing_core_count; };

    double get_ts_count() const noexcept { return true_sharing_count; };

    double get_ts_core_count() const noexcept { return true_sharing_core_count; };

    double get_fs_matrix_value(int a, int b) const noexcept { return false_sharing_matrix[a][b]; };

    double get_fs_core_matrix_value(int a, int b) const noexcept { return false_sharing_core_matrix[a][b]; };

    double get_ts_matrix_value(int a, int b) const noexcept { return true_sharing_matrix[a][b]; };	

    double get_ts_core_matrix_value(int a, int b) const noexcept { return true_sharing_core_matrix[a][b]; };

    bool has_events() const noexcept { return meta.has_events(); }

    void process(const adm_event_t& event) noexcept { meta.process(event); }

    void print() const noexcept;

    adm_object_t* min() noexcept
    {
      pthread_mutex_lock(&object_lock);
      //fprintf(stderr, "1 ");
      adm_object_t* m = this;
      while(m->left) m=m->left;
      //fprintf(stderr, "2 ");
      pthread_mutex_unlock(&object_lock);
      return m;
    }

    adm_object_t* find(const int object_id) noexcept
    {
      pthread_mutex_lock(&object_lock);
      //fprintf(stderr, "1 ");
      adm_object_t* f = this;
      while(f) {
        if(f->object_id == object_id) break;
        if(object_id < f->object_id) f = f->left;
        else f = f->right;
      }
      //fprintf(stderr, "2 ");
      pthread_mutex_unlock(&object_lock);
      return f;
    }

    void find_with_parent(const int object_id, adm_object_t*& p, adm_object_t*& f) noexcept
    {
      pthread_mutex_lock(&object_lock);
      //fprintf(stderr, "1 ");
      f = this; p = f->parent;
      while(f) {
        if(f->object_id == object_id) break;
        p = f;
        if(object_id < f->object_id) f = f->left;
        else f = f->right;
      }
      //fprintf(stderr, "2 ");
      pthread_mutex_unlock(&object_lock);
    }

    void insert(adm_object_t* a) noexcept
    {
      pthread_mutex_lock(&object_lock);
      //fprintf(stderr, "1 ");
      if(object_id>a->object_id) left = a;
      else right = a;
      a->parent = this;
      //fprintf(stderr, "2 ");
      pthread_mutex_unlock(&object_lock);
    }

    adm_object_t* splay() noexcept
    {
      pthread_mutex_lock(&object_lock);
      //fprintf(stderr, "1 ");
      while(parent) {
        adm_object_t* granpa = parent->parent;

        if(granpa) {
          if(parent->right==this) {
            if(granpa->left==parent) zagzig(granpa);
            else zagzag(granpa);
          } else {
            if(granpa->left==parent) zigzig(granpa);
            else zigzag(granpa);
          }
        }
        else {
          if(parent->left==this) zig();
          else zag();
        }
      }
      //fprintf(stderr, "2 ");
      pthread_mutex_unlock(&object_lock);
      return this;
    }

    void postorder(int indent)
    {
	if(this->left != nullptr) this->left->postorder(indent+4);
        if(this->right != nullptr) this->right->postorder(indent+4);
        if (indent) {
            std::cout << std::setw(indent) << ' ';
        }
        std::cout<< "id: " << this->object_id << " comm count: " << this->get_fs_count() + this->get_ts_count() << "\n ";
    }

};
 
ADM_VISIBILITY
adm_object_t* adm_db_insert(const uint64_t address, const uint64_t size, const int object_id, const state_t state=ADM_STATE_STATIC) noexcept;

ADM_VISIBILITY
adm_object_t* adm_db_insert_by_object_id(const int object_id, const state_t state) noexcept;

ADM_VISIBILITY
adm_object_t* adm_db_find_by_address(const uint64_t address) noexcept;

ADM_VISIBILITY
void adm_db_update_size(const uint64_t address, const uint64_t size) noexcept;

ADM_VISIBILITY
void adm_db_update_state(const uint64_t address, const state_t state) noexcept;

ADM_VISIBILITY
void adm_db_init() noexcept;

ADM_VISIBILITY
void adm_db_fini() noexcept;

static inline void adm_meta_init() noexcept {};

static inline void adm_meta_fini() noexcept {};

ADM_VISIBILITY
void adm_db_print(char output_directory[], const char * executable_name, int pid) noexcept;

ADM_VISIBILITY
char* adm_db_get_var_name(uint64_t address) noexcept;

ADM_VISIBILITY
adm_object_t* adm_db_find_by_object_id(const int object_id) noexcept;

}

#endif

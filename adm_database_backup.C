#include <cstdint>
#include <cstring>
#include <iostream>

#include <adm_config.h>
#include <adm_common.h>
#include <adm_splay.h>
#include <adm_memory.h>
#include <adm_database.h>

using namespace adamant;

static adm_splay_tree_t* tree = nullptr;
static pool_t<adm_splay_tree_t, ADM_DB_OBJ_BLOCKSIZE>* nodes = nullptr;
static pool_t<adm_object_t, ADM_DB_OBJ_BLOCKSIZE>* objects = nullptr;
extern int matrix_size;
extern int core_matrix_size;

struct sharing_struct {
  uint64_t address;
  double count;
} object_ts_count[100], object_fs_count[100], object_comm_count[100], object_ts_core_count[100], object_fs_core_count[100], object_comm_core_count[100];

static inline
adm_splay_tree_t* adm_db_find_node(const uint64_t address) noexcept
{
  if(tree)
    return tree->find(address);
  return nullptr;
}

ADM_VISIBILITY
adm_object_t* adamant::adm_db_find(const uint64_t address) noexcept
{
  adm_splay_tree_t* node = adm_db_find_node(address);
  if(node) return node->object;
  return nullptr;
}

ADM_VISIBILITY
adm_object_t* adamant::adm_db_insert(const uint64_t address, const uint64_t size, const state_t state) noexcept
{
  adm_splay_tree_t* obj = nullptr;
  adm_splay_tree_t* pos = nullptr;

  if(tree) tree->find_with_parent(address, pos, obj);
  if(obj==nullptr) {
    obj = nodes->malloc();
    if(obj==nullptr) return nullptr;

    obj->object = objects->malloc();
    if(obj->object==nullptr) return nullptr;

    obj->start = address;
    obj->object->set_address(address); 
    obj->end = obj->start+size;
    obj->object->set_size(size);
    obj->object->set_state(state);
    if(pos!=nullptr)
      pos->insert(obj);
    tree = obj->splay();
  }
  else {
    if(!(obj->object->get_state()&ADM_STATE_FREE)) {
      if(obj->start==address)
        adm_warning("db_insert: address already in tree and not free - ", address);
      else if(obj->start<address && address<obj->end)
        adm_warning("db_insert: address in range of another address in tree - ", obj->start, "..", obj->end, " (", address, ")");
      if(obj->end<address+size) {
        obj->end = address+size;
        obj->object->set_size(size);
      }
      tree = obj->splay();
    }
    else {
      obj->object = objects->malloc();
      if(obj->object==nullptr) return nullptr;

      obj->start = address;
      obj->object->set_address(address); 
      obj->end = obj->start+size;
      obj->object->set_size(size);
      obj->object->set_state(state);
      tree = obj->splay();
    }
  }

  return obj->object;
}

ADM_VISIBILITY
void adamant::adm_db_update_size(const uint64_t address, const uint64_t size) noexcept
{
  adm_splay_tree_t* obj = adm_db_find_node(address);
  if(obj) {
    obj->object->set_size(size);
    if(obj->start!=address) {
      adm_warning("db_update_size: address in range of another address in tree - ", obj->start, "..", obj->end, "(", address, ")");
      obj->start = address;
      obj->object->set_address(address);
    }
    obj->end = address+size;
    obj->object->set_size(size);
    tree = obj->splay();
  }
  else adm_warning("db_update_size: address not in tree - ", address);
}

ADM_VISIBILITY
void adamant::adm_db_update_state(const uint64_t address, const state_t state) noexcept
{
  adm_splay_tree_t* obj = adm_db_find_node(address);
  if(obj) {
    obj->object->add_state(state);
    if(obj->start!=address)
      adm_warning("db_update_state: address in range of another address in tree - ", obj->start, "..", obj->end, "(", address, ")");
  }
  else adm_warning("db_update_state: address not in tree - ", address);
}

static int min(int a, int b)
{
  if(a < b)
    return a;
  else
    return b;
}

ADM_VISIBILITY
void adamant::adm_db_print() noexcept
{
  bool all = adm_conf_string("+all", "1");
  pool_t<adm_splay_tree_t, ADM_DB_OBJ_BLOCKSIZE>::iterator n(*nodes);
  int i = 0;
  int object_count = 0;
  for(adm_splay_tree_t* obj = n.next(); obj!=nullptr; obj = n.next()) {
    //if(all || obj->object->has_events())
      if(i < 100) {
	object_ts_count[i].address = obj->object->get_address();
	object_ts_count[i].count = obj->object->get_ts_count();

	object_ts_core_count[i].address = obj->object->get_address();
        object_ts_core_count[i].count = obj->object->get_ts_core_count();

	object_fs_count[i].address = obj->object->get_address();
	object_fs_count[i].count = obj->object->get_fs_count();

	object_fs_core_count[i].address = obj->object->get_address();
        object_fs_core_count[i].count = obj->object->get_fs_core_count();

	object_comm_count[i].address = obj->object->get_address();
	object_comm_count[i].count = obj->object->get_ts_count() + obj->object->get_fs_count();

	object_comm_core_count[i].address = obj->object->get_address();
        object_comm_core_count[i].count = obj->object->get_ts_core_count() + obj->object->get_fs_core_count();
      }
      obj->object->print();
      i++;
   }
   object_count = min(100, i);
  //uint64_t top_5_object_ts_count[5][2] = { 0 };
  // begin here
  struct sharing_struct top_5_object_ts_count[5];

  for(i = 0; i < 5; i++) {
    top_5_object_ts_count[i].address = 0;
    top_5_object_ts_count[i].count = 0;
  }

  for(i = 0; i < 5; i++) {
    uint64_t largest = 0;
    uint64_t top_address = 0;
    uint64_t top_index = 0;
    for(int j = 0; j < object_count; j++) {
      if(largest < object_ts_count[j].count && object_ts_count[j].address != 0) {
	top_address = object_ts_count[j].address;
	top_index = j;
	largest = object_ts_count[j].count;
      }
    }
    if(top_address == 0)
      break;
    top_5_object_ts_count[i].address = top_address;
    top_5_object_ts_count[i].count = largest;
    object_ts_count[top_index].address = 0;
  }
  printf("top 5 objects with largest true sharing count\n");
  for(i = 0; i < 5; i++) {
    if(top_5_object_ts_count[i].address)
      {
	// dump fs matrix
	adm_object_t* obj = adm_db_find(top_5_object_ts_count[i].address);
	FILE * fp;

	char * var_name = static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]);
	char file_name[100];
	char append[100];
	strcpy (file_name, var_name);
	sprintf(append,".ts_matrix_rank_%d.csv", i);
	strcat(file_name, append);
	fp = fopen (file_name, "w+");
        printf("largest %d: address: %lx count: %0.2lf, from variable %s\n", i, top_5_object_ts_count[i].address, top_5_object_ts_count[i].count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
	for(int m=matrix_size; m >= 0; m--) {
	  for(int n=0; n <= matrix_size; n++) {
	    fprintf(fp, "%ld,", obj->get_ts_matrix_value(m, n));
	    //printf("%ld ", obj->get_ts_matrix_value(m, n));
	  }
	  fprintf(fp,"\n");
	  //printf("\n");
	}
	fclose(fp);
      }
  }
  
  // until here

// begin here
  struct sharing_struct top_5_object_ts_core_count[5];

  for(i = 0; i < 5; i++) {
    top_5_object_ts_core_count[i].address = 0;
    top_5_object_ts_core_count[i].count = 0;
  }

  for(i = 0; i < 5; i++) {
    uint64_t largest = 0;
    uint64_t top_address = 0;
    uint64_t top_index = 0;
    for(int j = 0; j < object_count; j++) {
      if(largest < object_ts_core_count[j].count && object_ts_core_count[j].address != 0) {
	top_address = object_ts_core_count[j].address;
	top_index = j;
	largest = object_ts_core_count[j].count;
      }
    }
    if(top_address == 0)
      break;
    top_5_object_ts_core_count[i].address = top_address;
    top_5_object_ts_core_count[i].count = largest;
    object_ts_core_count[top_index].address = 0;
  }
  printf("top 5 objects with largest true sharing count between cores\n");
  for(i = 0; i < 5; i++) {
    if(top_5_object_ts_core_count[i].address)
      {
	// dump fs matrix
	adm_object_t* obj = adm_db_find(top_5_object_ts_core_count[i].address);
	FILE * fp;

	char * var_name = static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]);
	char file_name[100];
	char append[100];
	strcpy (file_name, var_name);
	sprintf(append,".ts_core_matrix_rank_%d.csv", i);
	strcat(file_name, append);
	fp = fopen (file_name, "w+");
        printf("largest %d: address: %lx count: %0.2lf, from variable %s\n", i, top_5_object_ts_core_count[i].address, top_5_object_ts_core_count[i].count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
	for(int m=core_matrix_size; m >= 0; m--) {
	  for(int n=0; n <= core_matrix_size; n++) {
	    fprintf(fp, "%ld,", obj->get_ts_core_matrix_value(m, n));
	    //printf("%ld ", obj->get_ts_matrix_value(m, n));
	  }
	  fprintf(fp,"\n");
	  //printf("\n");
	}
	fclose(fp);
      }
  }
  
  // until here

// begin
  struct sharing_struct top_5_object_fs_count[5];
  for(i = 0; i < 5; i++) {
    top_5_object_fs_count[i].address = 0;
    top_5_object_fs_count[i].count = 0;
  }
  
  for(i = 0; i < 5; i++) {
    uint64_t largest = 0;
    uint64_t top_address = 0;
    uint64_t top_index = 0;
    for(int j = 0; j < object_count; j++) {
      if(largest < object_fs_count[j].count && object_fs_count[j].address != 0) {
	top_address = object_fs_count[j].address;
	top_index = j;
	largest = object_fs_count[j].count;
      }
    }
    if(top_address == 0)
      break;
    top_5_object_fs_count[i].address = top_address;
    top_5_object_fs_count[i].count = largest;
    object_fs_count[top_index].address = 0;
  }
  printf("top 5 objects with largest false sharing count\n");
  for(i = 0; i < 5; i++) {
    if(top_5_object_fs_count[i].address)
      {
	// dump fs matrix
	adm_object_t* obj = adm_db_find(top_5_object_fs_count[i].address);
	FILE * fp;

	char * var_name = static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]);
	char file_name[100];
	char append[100];
	strcpy (file_name, var_name);
	sprintf(append,".fs_matrix_rank_%d.csv", i);
	strcat(file_name, append);
	fp = fopen (file_name, "w+");
        printf("largest %d: address: %lx count: %0.2lf, from variable %s\n", i, top_5_object_fs_count[i].address, top_5_object_fs_count[i].count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
	for(int m=matrix_size; m >= 0; m--) {
	  for(int n=0; n <= matrix_size; n++) {
	    fprintf(fp, "%0.2lf,", obj->get_fs_matrix_value(m, n));
	    //printf("%ld ", obj->get_fs_matrix_value(m, n));
	  }
	  fprintf(fp,"\n");
	  //printf("\n");
	}
	fclose(fp);
      }
  }
// end  

// begin
  struct sharing_struct top_5_object_fs_core_count[5];
  for(i = 0; i < 5; i++) {
    top_5_object_fs_core_count[i].address = 0;
    top_5_object_fs_core_count[i].count = 0;
  }
  
  for(i = 0; i < 5; i++) {
    uint64_t largest = 0;
    uint64_t top_address = 0;
    uint64_t top_index = 0;
    for(int j = 0; j < object_count; j++) {
      if(largest < object_fs_core_count[j].count && object_fs_core_count[j].address != 0) {
	top_address = object_fs_core_count[j].address;
	top_index = j;
	largest = object_fs_core_count[j].count;
      }
    }
    if(top_address == 0)
      break;
    top_5_object_fs_core_count[i].address = top_address;
    top_5_object_fs_core_count[i].count = largest;
    object_fs_core_count[top_index].address = 0;
  }
  printf("top 5 objects with largest false sharing count between cores\n");
  for(i = 0; i < 5; i++) {
    if(top_5_object_fs_core_count[i].address)
      {
	// dump fs matrix
	adm_object_t* obj = adm_db_find(top_5_object_fs_core_count[i].address);
	FILE * fp;

	char * var_name = static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]);
	char file_name[100];
	char append[100];
	strcpy (file_name, var_name);
	sprintf(append,".fs_core_matrix_rank_%d.csv", i);
	strcat(file_name, append);
	fp = fopen (file_name, "w+");
        printf("largest %d: address: %lx count: %0.2lf, from variable %s\n", i, top_5_object_fs_core_count[i].address, top_5_object_fs_core_count[i].count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
	for(int m=core_matrix_size; m >= 0; m--) {
	  for(int n=0; n <= core_matrix_size; n++) {
	    fprintf(fp, "%0.2lf,", obj->get_fs_core_matrix_value(m, n));
	    //printf("%ld ", obj->get_fs_matrix_value(m, n));
	  }
	  fprintf(fp,"\n");
	  //printf("\n");
	}
	fclose(fp);
      }
  }
// end 

  // before
  struct sharing_struct top_5_object_comm_count[5];
  for(i = 0; i < 5; i++) {
    top_5_object_comm_count[i].address = 0;
    top_5_object_comm_count[i].count = 0;
  }
  
  for(i = 0; i < 5; i++) {
    uint64_t largest = 0;
    uint64_t top_address = 0;
    uint64_t top_index = 0;
    for(int j = 0; j < object_count; j++) {
      if(largest < object_comm_count[j].count && object_comm_count[j].address != 0) {
	top_address = object_comm_count[j].address;
	top_index = j;
	largest = object_comm_count[j].count;
      }
    }
    if(top_address == 0)
      break;
    top_5_object_comm_count[i].address = top_address;
    top_5_object_comm_count[i].count = largest;
    object_comm_count[top_index].address = 0;
  }
  printf("top 5 objects with largest all sharing count\n");
  for(i = 0; i < 5; i++) {
    if(top_5_object_comm_count[i].address)
      {
	// dump fs matrix
	adm_object_t* obj = adm_db_find(top_5_object_comm_count[i].address);
	FILE * fp;

	char * var_name = static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]);
	char file_name[100];
	char append[100];
	strcpy (file_name, var_name);
	sprintf(append,".comm_matrix_rank_%d.csv", i);
	strcat(file_name, append);
	fp = fopen (file_name, "w+");
        printf("largest %d: address: %lx count: %0.2lf, from variable %s\n", i, top_5_object_comm_count[i].address, top_5_object_comm_count[i].count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
	for(int m=matrix_size; m >= 0; m--) {
	  for(int n=0; n <= matrix_size; n++) {
	    fprintf(fp, "%0.2lf,", (double) obj->get_ts_matrix_value(m, n) + obj->get_fs_matrix_value(m, n));
	    //printf("%ld ", obj->get_ts_matrix_value(m, n) + obj->get_fs_matrix_value(m, n));
	  }
	  fprintf(fp,"\n");
	  //printf("\n");
	}
	fclose(fp);
      }
  }
  // after

  // before
  struct sharing_struct top_5_object_comm_core_count[5];
  for(i = 0; i < 5; i++) {
    top_5_object_comm_core_count[i].address = 0;
    top_5_object_comm_core_count[i].count = 0;
  }
  
  for(i = 0; i < 5; i++) {
    uint64_t largest = 0;
    uint64_t top_address = 0;
    uint64_t top_index = 0;
    for(int j = 0; j < object_count; j++) {
      if(largest < object_comm_core_count[j].count && object_comm_core_count[j].address != 0) {
	top_address = object_comm_core_count[j].address;
	top_index = j;
	largest = object_comm_core_count[j].count;
      }
    }
    if(top_address == 0)
      break;
    top_5_object_comm_core_count[i].address = top_address;
    top_5_object_comm_core_count[i].count = largest;
    object_comm_core_count[top_index].address = 0;
  }
  printf("top 5 objects with largest all sharing count between cores\n");
  for(i = 0; i < 5; i++) {
    if(top_5_object_comm_core_count[i].address)
      {
	// dump fs matrix
	adm_object_t* obj = adm_db_find(top_5_object_comm_core_count[i].address);
	FILE * fp;

	char * var_name = static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]);
	char file_name[100];
	char append[100];
	strcpy (file_name, var_name);
	sprintf(append,".comm_matrix_rank_%d.csv", i);
	strcat(file_name, append);
	fp = fopen (file_name, "w+");
        printf("largest %d: address: %lx count: %0.2lf, from variable %s\n", i, top_5_object_comm_core_count[i].address, top_5_object_comm_core_count[i].count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
	for(int m=core_matrix_size; m >= 0; m--) {
	  for(int n=0; n <= core_matrix_size; n++) {
	    fprintf(fp, "%0.2lf,", (double) obj->get_ts_core_matrix_value(m, n) + obj->get_fs_core_matrix_value(m, n));
	    //printf("%ld ", obj->get_ts_matrix_value(m, n) + obj->get_fs_matrix_value(m, n));
	  }
	  fprintf(fp,"\n");
	  //printf("\n");
	}
	fclose(fp);
      }
  }
  // after

  // extract top 5 ts
  // extract top 5 fs
  // extract top 5 comm
}

ADM_VISIBILITY
void adamant::adm_db_init() noexcept
{
  nodes = new pool_t<adm_splay_tree_t, ADM_DB_OBJ_BLOCKSIZE>;
  objects = new pool_t<adm_object_t, ADM_DB_OBJ_BLOCKSIZE>;
}

ADM_VISIBILITY
void adamant::adm_db_fini() noexcept
{
  delete nodes;
  delete objects;
}


ADM_VISIBILITY
void adm_object_t::print() const noexcept
{
  uint64_t a = get_address();
  adm_out(&a, sizeof(a));
  uint64_t z = get_size();
  adm_out(&z, sizeof(z));
  state_t s = get_state();
  adm_out(&s, sizeof(s));
  meta.print();
}

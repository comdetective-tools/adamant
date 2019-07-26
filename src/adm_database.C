#include <cstdint>
#include <cstring>
#include <iostream>

#include <adm_config.h>
#include <adm_common.h>
#include <adm_splay.h>
#include <adm_memory.h>
#include <adm_database.h>
#include<bits/stdc++.h>


using namespace std; 
using namespace adamant;

static adm_splay_tree_t* tree = nullptr;
static pool_t<adm_splay_tree_t, ADM_DB_OBJ_BLOCKSIZE>* nodes = nullptr;
static pool_t<adm_object_t, ADM_DB_OBJ_BLOCKSIZE>* objects = nullptr;
extern int matrix_size;
extern int core_matrix_size;

/*
struct sharing_struct {
  uint64_t address;
  double count;
} object_ts_count[100], object_fs_count[100], object_comm_count[100], object_ts_core_count[100], object_fs_core_count[100], object_comm_core_count[100];*/

struct shared_object {
    uint64_t address; 
    uint64_t size;
    double count; 
      
    // This function is used by set to order 
    // elements of Test. 
    bool operator<(const shared_object& s) const
    { 
        return (this->count > s.count); 
    } 
};

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
void adamant::adm_db_print(char output_directory[], const char * executable_name, int pid ) noexcept
{
	set<struct shared_object> object_ts_count; 
	set<struct shared_object> object_ts_core_count;

	set<struct shared_object> object_fs_count; 
	set<struct shared_object> object_fs_core_count;

	set<struct shared_object> object_comm_count; 
	set<struct shared_object> object_comm_core_count;

	bool all = adm_conf_string("+all", "1");
	pool_t<adm_splay_tree_t, ADM_DB_OBJ_BLOCKSIZE>::iterator n(*nodes);
	int i = 0;
	int object_count = 0;
	//fprintf(stderr, "reach 1");
	for(adm_splay_tree_t* obj = n.next(); obj!=nullptr; obj = n.next()) {
		//fprintf(stderr, "iteration begins\n");
		shared_object ts_node = { obj->object->get_address(), obj->object->get_size(), obj->object->get_ts_count() };
		object_ts_count.insert(ts_node);
		//fprintf(stderr, "iteration mids\n");
		shared_object ts_core_node = { obj->object->get_address(),obj->object->get_size(), obj->object->get_ts_core_count() };
		object_ts_core_count.insert(ts_core_node);

		shared_object fs_node = { obj->object->get_address(),obj->object->get_size(), obj->object->get_fs_count() };
		object_fs_count.insert(fs_node);

		//fprintf(stderr, "iteration mids 1/2\n");

		shared_object fs_core_node = { obj->object->get_address(),obj->object->get_size(), obj->object->get_fs_core_count() };
		object_fs_core_count.insert(fs_core_node);

		shared_object comm_node = { obj->object->get_address(),obj->object->get_size(), obj->object->get_fs_count() + obj->object->get_ts_count() };
		object_comm_count.insert(comm_node);

		shared_object comm_core_node = { obj->object->get_address(),obj->object->get_size(), obj->object->get_fs_core_count() + obj->object->get_ts_core_count() };
		object_comm_core_count.insert(comm_core_node);
		//fprintf(stderr, "iteration mids 2\n");
		//obj->object->print();
		object_count++;
		//fprintf(stderr, "iteration ends\n");
		//fprintf(stderr, "object count: %d\n", object_count);
  	}
	//fprintf(stderr, "reach 2");
   	set<struct shared_object>::iterator it;  
  	
	FILE * ts_fp;
	char ts_file_name[PATH_MAX];
	sprintf(ts_file_name, "%s/%s-%d-ts_object_ranking.txt", output_directory, executable_name, pid );
	ts_fp = fopen (ts_file_name, "w+");    
	//fprintf(stderr, "reach 3");
	for (it = object_ts_count.begin(), i = 0; it != object_ts_count.end() && i < 50; it++, i++) 
	{  
		adm_object_t* obj = adm_db_find((*it).address); 
		FILE * fp;

		//char * var_name = static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]);
		char file_name[PATH_MAX];
		char append[100];
		char address_index[20];
		sprintf(address_index,"%lx", (*it).address);
		//strcpy (file_name, address_index);
		sprintf(append,".ts_matrix_rank_%d.csv", i);
		//strcat(file_name, append);
		sprintf(file_name, "%s/%s-%d-%s-%s", output_directory, executable_name, pid, address_index, append );
		fp = fopen (file_name, "w+");
        	fprintf(ts_fp, "largest %d: address: %lx size: %ld, count: %0.2lf, from variable %s ", i, (*it).address, (*it).size, (*it).count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
		// before

		const adamant::stack_t* stack = static_cast<const adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]);
  		if(obj->meta.meta[ADM_META_STACK_TYPE] != nullptr) {
    			uint32_t func = 0;
    			for(uint8_t f=0; f < ADM_META_STACK_DEPTH && stack->ip[f]; ++f) {
      				size_t len = strlen(stack->function+func)+1;
      				fprintf(ts_fp, "%s ", stack->function+func);
      				func += len;
    			}
  		}
		fprintf(ts_fp, "\n");
 
		// after

		for(int m=matrix_size; m >= 0; m--) {
	  		for(int n=0; n <= matrix_size; n++) {
	    			if(n < matrix_size)
	    				fprintf(fp, "%0.2lf,", obj->get_ts_matrix_value(m, n) + obj->get_ts_matrix_value(n, m));
	    			else
					fprintf(fp, "%0.2lf", obj->get_ts_matrix_value(m, n) + obj->get_ts_matrix_value(n, m));
	  		}
	  		fprintf(fp,"\n");
		}
		fclose(fp);
    	} 
	fclose(ts_fp);

	FILE * ts_core_fp;
	char ts_core_file_name[PATH_MAX];
	sprintf(ts_core_file_name, "%s/%s-%d-ts_core_object_ranking.txt", output_directory, executable_name, pid );
	ts_core_fp = fopen (ts_core_file_name, "w+");    

	for (it = object_ts_core_count.begin(), i = 0; it != object_ts_core_count.end() && i < 50; it++, i++) 
	{  
		adm_object_t* obj = adm_db_find((*it).address); 
		FILE * fp;

		char file_name[PATH_MAX];
		char append[100];
		char address_index[20];
		sprintf(address_index,"%lx", (*it).address);
		//strcpy (file_name, address_index);
		sprintf(append,".ts_core_matrix_rank_%d.csv", i);
		//strcat(file_name, append);
		sprintf(file_name, "%s/%s-%d-%s-%s", output_directory, executable_name, pid, address_index, append );
		fp = fopen (file_name, "w+");
        	fprintf(ts_core_fp, "largest %d: address: %lx size: %ld, count: %0.2lf, from variable %s ", i, (*it).address, (*it).size, (*it).count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
		// before

		const adamant::stack_t* stack = static_cast<const adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]);
  		if(obj->meta.meta[ADM_META_STACK_TYPE] != nullptr) {
    			uint32_t func = 0;
    			for(uint8_t f=0; f < ADM_META_STACK_DEPTH && stack->ip[f]; ++f) {
      				size_t len = strlen(stack->function+func)+1;
      				fprintf(ts_core_fp, "%s ", stack->function+func);
      				func += len;
    			}
  		}
		fprintf(ts_core_fp, "\n");
 
		// after

		for(int m=core_matrix_size; m >= 0; m--) {
	  		for(int n=0; n <= core_matrix_size; n++) {
	    			if(n < core_matrix_size)
	    				fprintf(fp, "%0.2lf,", obj->get_ts_core_matrix_value(m, n) + obj->get_ts_core_matrix_value(n, m));
	    			else
					fprintf(fp, "%0.2lf", obj->get_ts_core_matrix_value(m, n) + obj->get_ts_core_matrix_value(n, m));
	  		}
	  		fprintf(fp,"\n");
		}
		fclose(fp);
    	} 
	fclose(ts_core_fp);
	
	// before
	FILE * fs_fp;
	char fs_file_name[PATH_MAX];
	sprintf(fs_file_name, "%s/%s-%d-fs_object_ranking.txt", output_directory, executable_name, pid );
	fs_fp = fopen (fs_file_name, "w+");    

	for (it = object_fs_count.begin(), i = 0; it != object_fs_count.end() && i < 50; it++, i++) 
	{  
		adm_object_t* obj = adm_db_find((*it).address); 
		FILE * fp;

		char file_name[PATH_MAX];
		char append[100];
		char address_index[20];
		sprintf(address_index,"%lx", (*it).address);
		//strcpy (file_name, address_index);
		sprintf(append,".fs_matrix_rank_%d.csv", i);
		//strcat(file_name, append);
		sprintf(file_name, "%s/%s-%d-%s-%s", output_directory, executable_name, pid, address_index, append );
		fp = fopen (file_name, "w+");
        	fprintf(fs_fp, "largest %d: address: %lx size: %ld, count: %0.2lf, from variable %s ", i, (*it).address, (*it).size, (*it).count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
		// before

		const adamant::stack_t* stack = static_cast<const adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]);
  		if(obj->meta.meta[ADM_META_STACK_TYPE] != nullptr) {
    			uint32_t func = 0;
    			for(uint8_t f=0; f < ADM_META_STACK_DEPTH && stack->ip[f]; ++f) {
      				size_t len = strlen(stack->function+func)+1;
      				fprintf(fs_fp, "%s ", stack->function+func);
      				func += len;
    			}
  		}
		fprintf(fs_fp, "\n");
 
		// after

		for(int m=matrix_size; m >= 0; m--) {
	  		for(int n=0; n <= matrix_size; n++) {
	    			if(n < matrix_size)
	    				fprintf(fp, "%0.2lf,", obj->get_fs_matrix_value(m, n) + obj->get_fs_matrix_value(n, m));
	    			else
					fprintf(fp, "%0.2lf", obj->get_fs_matrix_value(m, n) + obj->get_fs_matrix_value(n, m));
	  		}
	  		fprintf(fp,"\n");
		}
		fclose(fp);
    	} 
	fclose(fs_fp);

	FILE * fs_core_fp;
	char fs_core_file_name[PATH_MAX];
	sprintf(fs_core_file_name, "%s/%s-%d-fs_core_object_ranking.txt", output_directory, executable_name, pid );
	fs_core_fp = fopen (fs_core_file_name, "w+");    

	for (it = object_fs_core_count.begin(), i = 0; it != object_fs_core_count.end() && i < 50; it++, i++) 
	{  
		adm_object_t* obj = adm_db_find((*it).address); 
		FILE * fp;

		char file_name[PATH_MAX];
		char append[100];
		char address_index[20];
		sprintf(address_index,"%lx", (*it).address);
		//strcpy (file_name, address_index);
		sprintf(append,".fs_core_matrix_rank_%d.csv", i);
		//strcat(file_name, append);
		sprintf(file_name, "%s/%s-%d-%s-%s", output_directory, executable_name, pid, address_index, append );
		fp = fopen (file_name, "w+");
        	fprintf(fs_core_fp, "largest %d: address: %lx size: %ld, count: %0.2lf, from variable %s ", i, (*it).address, (*it).size, (*it).count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
		// before

		const adamant::stack_t* stack = static_cast<const adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]);
  		if(obj->meta.meta[ADM_META_STACK_TYPE] != nullptr) {
    			uint32_t func = 0;
    			for(uint8_t f=0; f < ADM_META_STACK_DEPTH && stack->ip[f]; ++f) {
      				size_t len = strlen(stack->function+func)+1;
      				fprintf(fs_core_fp, "%s ", stack->function+func);
      				func += len;
    			}
  		}
		fprintf(fs_core_fp, "\n");
 
		// after

		for(int m=core_matrix_size; m >= 0; m--) {
	  		for(int n=0; n <= core_matrix_size; n++) {
	    			if(n < core_matrix_size)
	    				fprintf(fp, "%0.2lf,", obj->get_fs_core_matrix_value(m, n) + obj->get_fs_core_matrix_value(n, m));
	    			else
					fprintf(fp, "%0.2lf", obj->get_fs_core_matrix_value(m, n) + obj->get_fs_core_matrix_value(n, m));
	  		}
	  		fprintf(fp,"\n");
		}
		fclose(fp);
    	} 
	fclose(fs_core_fp);
	// after

	// before
	FILE * comm_fp;
	char comm_file_name[PATH_MAX];
	sprintf(comm_file_name, "%s/%s-%d-as_object_ranking.txt", output_directory, executable_name, pid );
	comm_fp = fopen (comm_file_name, "w+");    

	for (it = object_comm_count.begin(), i = 0; it != object_comm_count.end() && i < 50; it++, i++) 
	{  
		adm_object_t* obj = adm_db_find((*it).address); 
		FILE * fp;


		char file_name[PATH_MAX];
		char append[100];
		char address_index[20];
		sprintf(address_index,"%lx", (*it).address);
		//strcpy (file_name, address_index);
		sprintf(append,".as_matrix_rank_%d.csv", i);
		//strcat(file_name, append);
		sprintf(file_name, "%s/%s-%d-%s-%s", output_directory, executable_name, pid, address_index, append );

		fp = fopen (file_name, "w+");
        	fprintf(comm_fp, "largest %d: address: %lx size: %ld, count: %0.2lf, from variable %s ", i, (*it).address, (*it).size, (*it).count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
		// before

		const adamant::stack_t* stack = static_cast<const adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]);
  		if(obj->meta.meta[ADM_META_STACK_TYPE] != nullptr) {
    			uint32_t func = 0;
    			for(uint8_t f=0; f < ADM_META_STACK_DEPTH && stack->ip[f]; ++f) {
      				size_t len = strlen(stack->function+func)+1;
      				fprintf(comm_fp, "%s ", stack->function+func);
      				func += len;
    			}
  		}
		fprintf(comm_fp, "\n");
 
		// after

		for(int m=matrix_size; m >= 0; m--) {
	  		for(int n=0; n <= matrix_size; n++) {
	    			if(n < matrix_size)
	    				fprintf(fp, "%0.2lf,", obj->get_fs_matrix_value(m, n) + obj->get_fs_matrix_value(n, m) + obj->get_ts_matrix_value(m, n) + obj->get_ts_matrix_value(n, m));
	    			else
					fprintf(fp, "%0.2lf", obj->get_fs_matrix_value(m, n) + obj->get_fs_matrix_value(n, m) + obj->get_ts_matrix_value(m, n) + obj->get_ts_matrix_value(n, m));
	  		}
	  		fprintf(fp,"\n");
		}
		fclose(fp);
    	} 
	fclose(comm_fp);

	FILE * comm_core_fp;
	char comm_core_file_name[PATH_MAX];
	sprintf(comm_core_file_name, "%s/%s-%d-as_core_object_ranking.txt", output_directory, executable_name, pid );
	comm_core_fp = fopen (comm_core_file_name, "w+");    

	for (it = object_comm_core_count.begin(), i = 0; it != object_comm_core_count.end() && i < 50; it++, i++) 
	{  
		adm_object_t* obj = adm_db_find((*it).address); 
		FILE * fp;

		char file_name[PATH_MAX];
		char append[100];
		char address_index[20];
		sprintf(address_index,"%lx", (*it).address);
		//strcpy (file_name, address_index);
		sprintf(append,".as_core_matrix_rank_%d.csv", i);
		//strcat(file_name, append);
		sprintf(file_name, "%s/%s-%d-%s-%s", output_directory, executable_name, pid, address_index, append );
		fp = fopen (file_name, "w+");
        	fprintf(comm_core_fp, "largest %d: address: %lx size: %ld, count: %0.2lf, from variable %s ", i, (*it).address, (*it).size, (*it).count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
		// before

		const adamant::stack_t* stack = static_cast<const adamant::stack_t*>(obj->meta.meta[ADM_META_STACK_TYPE]);
  		if(obj->meta.meta[ADM_META_STACK_TYPE] != nullptr) {
    			uint32_t func = 0;
    			for(uint8_t f=0; f < ADM_META_STACK_DEPTH && stack->ip[f]; ++f) {
      				size_t len = strlen(stack->function+func)+1;
      				fprintf(comm_core_fp, "%s ", stack->function+func);
      				func += len;
    			}
  		}
		fprintf(comm_core_fp, "\n");
 
		// after

		for(int m=core_matrix_size; m >= 0; m--) {
	  		for(int n=0; n <= core_matrix_size; n++) {
	    			if(n < core_matrix_size)
	    				fprintf(fp, "%0.2lf,", obj->get_fs_core_matrix_value(m, n) + obj->get_fs_core_matrix_value(n, m));
	    			else
					fprintf(fp, "%0.2lf", obj->get_fs_core_matrix_value(m, n) + obj->get_fs_core_matrix_value(n, m));
	  		}
	  		fprintf(fp,"\n");
		}
		fclose(fp);
    	} 
	fclose(comm_core_fp);
	// after
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
  //adm_meta_print_stack
  //adm_meta_print_base

  meta.print();
}

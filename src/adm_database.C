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
static adm_object_t* object_tree = nullptr;
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
    int object_id; 
    //uint64_t size;
    double count; 
      
    // This function is used by set to order 
    // elements of Test. 
    bool operator<(const shared_object& s) const
    { 
        return (this->count > s.count); 
    } 
};

static inline
adm_splay_tree_t* adm_db_find_node_by_address(const uint64_t address) noexcept
{
  if(tree) {
    adm_splay_tree_t* node = tree->find(address);
    if(node) { 
	pthread_mutex_lock(&node_lock);
	tree = node->splay();
	pthread_mutex_unlock(&node_lock);   	
    	return node;
    } 
    return nullptr;
  }
  return nullptr;
}

ADM_VISIBILITY
adm_object_t* adamant::adm_db_find_by_object_id(const int object_id) noexcept
{
  if(object_tree) {
    adm_object_t* obj = object_tree->find(object_id);
    if(obj) {
	pthread_mutex_lock(&node_lock);
	object_tree = obj->splay();
	pthread_mutex_unlock(&node_lock);
    	return obj;
    }
    return nullptr;
  }
  return nullptr;
}

/*
ADM_VISIBILITY
adm_object_t* adamant::adm_db_find_by_address(const uint64_t address) noexcept
{
  adm_splay_tree_t* node = adm_db_find_node_by_address(address);
  if(node) return node->object;
  //fprintf(stderr, "this one is reached 0\n");
  if(object_tree) {
	adm_object_t* obj = adm_db_find_by_object_id(0);
	if(obj)
		return obj;
	obj = objects->malloc();
  	if(obj == nullptr) return nullptr;
  	obj->set_object_id(0);
  	pthread_mutex_lock(&node_lock);
  	object_tree = obj->splay();
  	pthread_mutex_unlock(&node_lock);
  }
  //return nullptr;
  //fprintf(stderr, "this one is reached\n");
  adm_object_t* obj = objects->malloc();
  if(obj == nullptr) return nullptr;
  obj->set_object_id(0);
  pthread_mutex_lock(&node_lock);
  object_tree = obj->splay();
  pthread_mutex_unlock(&node_lock);
}*/

ADM_VISIBILITY
adm_object_t* adamant::adm_db_insert(const uint64_t address, const uint64_t size, const int object_id, const state_t state) noexcept
{
  adm_splay_tree_t* obj = nullptr;
  adm_splay_tree_t* pos = nullptr;

  if(tree) tree->find_with_parent(address, pos, obj);
  if(obj==nullptr) {
    obj = nodes->malloc();
    if(obj==nullptr) return nullptr;
	// serach existing object by id first

    adm_object_t* target_object = nullptr;
    adm_object_t* parent_object = nullptr;

    if(object_tree) object_tree->find_with_parent(object_id, parent_object, target_object);

    if(target_object==nullptr) {
    	obj->object = objects->malloc();
    	if(obj->object==nullptr) return nullptr;
	obj->object->set_object_id(object_id);
	target_object = obj->object;
	if(parent_object!=nullptr)
      		parent_object->insert(target_object);
	pthread_mutex_lock(&node_lock);
	object_tree = target_object->splay();
	pthread_mutex_unlock(&node_lock);
    } else {
	obj->object = target_object;
	pthread_mutex_lock(&node_lock);
	object_tree = target_object->splay();
	pthread_mutex_unlock(&node_lock);
    }
    obj->start = address; 
    obj->end = obj->start+size;
    obj->set_size(size);
    obj->set_state(state);
    if(pos!=nullptr)
      pos->insert(obj);
    pthread_mutex_lock(&node_lock);
    tree = obj->splay();
    pthread_mutex_unlock(&node_lock);
  }
  else {
    if(!(obj->get_state()&ADM_STATE_FREE)) {
      if(obj->start==address)
        adm_warning("db_insert: address already in tree and not free - ", address);
      else if(obj->start<address && address<obj->end)
        adm_warning("db_insert: address in range of another address in tree - ", obj->start, "..", obj->end, " (", address, ")");
      if(obj->end<address+size) {
        obj->end = address+size;
        obj->set_size(size);
      }
      pthread_mutex_lock(&node_lock);
      tree = obj->splay();
      pthread_mutex_unlock(&node_lock);
    }
    else {
      //obj->object = objects->malloc();
      if(obj->object==nullptr) return nullptr;

      obj->start = address;

      adm_object_t* target_object = nullptr;
      adm_object_t* parent_object = nullptr;

      if(object_tree) object_tree->find_with_parent(object_id, parent_object, target_object);

      if(target_object==nullptr) {
    	obj->object = objects->malloc();
    	if(obj->object==nullptr) return nullptr;
	obj->object->set_object_id(object_id);
	target_object = obj->object;
	if(parent_object!=nullptr)
      		parent_object->insert(target_object);
	pthread_mutex_lock(&node_lock);
	object_tree = target_object->splay();
	pthread_mutex_unlock(&node_lock);
      } else {
	obj->object = target_object;
	pthread_mutex_lock(&node_lock);
	object_tree = target_object->splay();
	pthread_mutex_unlock(&node_lock);
      }
      obj->end = obj->start+size;
      obj->set_size(size);
      obj->set_state(state);
      pthread_mutex_lock(&node_lock);
      tree = obj->splay();
      pthread_mutex_unlock(&node_lock);
    }
  }

  return obj->object;
}

ADM_VISIBILITY
adm_object_t* adamant::adm_db_insert_by_object_id(const int object_id, const state_t state) noexcept
{

    adm_object_t* target_object = nullptr;
    adm_object_t* parent_object = nullptr;

    if(object_tree) object_tree->find_with_parent(object_id, parent_object, target_object);
    
    if(target_object==nullptr) {
    	adm_object_t* obj = objects->malloc();
    	if(obj==nullptr) return nullptr;
	obj->set_object_id(object_id);
	target_object = obj;
	if(parent_object!=nullptr)
      		parent_object->insert(target_object);
	pthread_mutex_lock(&node_lock);
	object_tree = target_object->splay();
	pthread_mutex_unlock(&node_lock);
    } else {
	adm_object_t* obj = target_object;
	pthread_mutex_lock(&node_lock);
	object_tree = target_object->splay();
	pthread_mutex_unlock(&node_lock);
    }
  

  return target_object;
}

ADM_VISIBILITY
adm_object_t* adamant::adm_db_find_by_address(const uint64_t address) noexcept
{
  adm_splay_tree_t* node = adm_db_find_node_by_address(address);
  if(node && !(node->get_state()&ADM_STATE_FREE)) return node->object;
  if(objects != nullptr)
  	return adm_db_insert(address, 8, 0, ADM_STATE_ALLOC);
  return nullptr;
}

ADM_VISIBILITY
void adamant::adm_db_update_size(const uint64_t address, const uint64_t size) noexcept
{
  adm_splay_tree_t* obj = adm_db_find_node_by_address(address);
  if(obj) {
    obj->set_size(size);
    if(obj->start!=address) {
      adm_warning("db_update_size: address in range of another address in tree - ", obj->start, "..", obj->end, "(", address, ")");
      obj->start = address;
    }
    obj->end = address+size;
    pthread_mutex_lock(&node_lock);
    tree = obj->splay();
    pthread_mutex_unlock(&node_lock);
  }
  else adm_warning("db_update_size: address not in tree - ", address);
}

ADM_VISIBILITY
void adamant::adm_db_update_state(const uint64_t address, const state_t state) noexcept
{
  adm_splay_tree_t* obj = adm_db_find_node_by_address(address);
  if(obj) {
    obj->add_state(state);
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
char* adamant::adm_db_get_var_name(uint64_t address) noexcept {
	adm_object_t* obj = adm_db_find_by_address(address);
	return static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]);
}

ADM_VISIBILITY
void adamant::adm_db_print(char output_directory[], const char * executable_name, int pid ) noexcept
{
	fprintf(stderr, "in adm_db_print\n");
	set<struct shared_object> object_ts_count; 
	set<struct shared_object> object_ts_core_count;

	set<struct shared_object> object_fs_count; 
	set<struct shared_object> object_fs_core_count;

	set<struct shared_object> object_comm_count; 
	set<struct shared_object> object_comm_core_count;

	bool all = adm_conf_string("+all", "1");
	pool_t<adm_object_t, ADM_DB_OBJ_BLOCKSIZE>::iterator n(*objects);
	int i = 0;
	int object_count = 0;
	//fprintf(stderr, "reach 1\n");
	for(adm_object_t* obj = n.next(); obj!=nullptr; obj = n.next()) {
		//fprintf(stderr, "iteration begins\n");
		shared_object ts_node = { obj->get_object_id(), obj->get_ts_count() };
		object_ts_count.insert(ts_node);
		//fprintf(stderr, "iteration mids\n");
		shared_object ts_core_node = { obj->get_object_id(), obj->get_ts_core_count() };
		object_ts_core_count.insert(ts_core_node);

		shared_object fs_node = { obj->get_object_id(), obj->get_fs_count() };
		object_fs_count.insert(fs_node);

		//fprintf(stderr, "iteration mids 1/2\n");

		shared_object fs_core_node = { obj->get_object_id(), obj->get_fs_core_count() };
		object_fs_core_count.insert(fs_core_node);

		shared_object comm_node = { obj->get_object_id(), obj->get_fs_count() + obj->get_ts_count() };
		object_comm_count.insert(comm_node);

		shared_object comm_core_node = { obj->get_object_id(), obj->get_fs_core_count() + obj->get_ts_core_count() };
		object_comm_core_count.insert(comm_core_node);
		//fprintf(stderr, "iteration mids 2\n");
		//obj->object->print();
		object_count++;
		//fprintf(stderr, "iteration ends\n");
		//fprintf(stderr, "object count: %d\n", object_count);
  	}
	//fprintf(stderr, "reach 2\n");
   	set<struct shared_object>::iterator it;  
  	
	FILE * ts_fp;
	char ts_file_name[PATH_MAX];
	sprintf(ts_file_name, "%s/%s-%d-ts_object_ranking.txt", output_directory, executable_name, pid );
	ts_fp = fopen (ts_file_name, "w+");    
	//fprintf(stderr, "reach 3\n");
	for (it = object_ts_count.begin(), i = 0; it != object_ts_count.end() && i < 50; it++, i++) 
	{  
		adm_object_t* obj = adm_db_find_by_object_id((*it).object_id); 
		FILE * fp;

		//char * var_name = static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]);
		char file_name[PATH_MAX];
		char append[100];
		//strcpy (file_name, address_index);
		sprintf(append,".ts_matrix_rank_%d.csv", i);
		//strcat(file_name, append);
		sprintf(file_name, "%s/%s-%d-%d-%s", output_directory, executable_name, pid, (*it).object_id, append );
		fp = fopen (file_name, "w+");
        	fprintf(ts_fp, "largest %d: object with id: %d, count: %0.2lf, from variable %s ", i, (*it).object_id, (*it).count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
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

	//fprintf(stderr, "reach 4\n");
	FILE * ts_core_fp;
	char ts_core_file_name[PATH_MAX];
	sprintf(ts_core_file_name, "%s/%s-%d-ts_core_object_ranking.txt", output_directory, executable_name, pid );
	ts_core_fp = fopen (ts_core_file_name, "w+");    
	//fprintf(stderr, "reach 5\n");
	for (it = object_ts_core_count.begin(), i = 0; it != object_ts_core_count.end() && i < 50; it++, i++) 
	{  
		adm_object_t* obj = adm_db_find_by_object_id((*it).object_id); 
		FILE * fp;

		char file_name[PATH_MAX];
		char append[100];
		//strcpy (file_name, address_index);
		sprintf(append,".ts_core_matrix_rank_%d.csv", i);
		//strcat(file_name, append);
		sprintf(file_name, "%s/%s-%d-%d-%s", output_directory, executable_name, pid, (*it).object_id, append );
		fp = fopen (file_name, "w+");
        	fprintf(ts_core_fp, "largest %d: object with id: %d, count: %0.2lf, from variable %s ", i, (*it).object_id, (*it).count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
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
	//fprintf(stderr, "reach 6\n");
	// before
	FILE * fs_fp;
	char fs_file_name[PATH_MAX];
	sprintf(fs_file_name, "%s/%s-%d-fs_object_ranking.txt", output_directory, executable_name, pid );
	fs_fp = fopen (fs_file_name, "w+");    
	//fprintf(stderr, "reach 7\n");
	for (it = object_fs_count.begin(), i = 0; it != object_fs_count.end() && i < 50; it++, i++) 
	{  
		adm_object_t* obj = adm_db_find_by_object_id((*it).object_id); 
		FILE * fp;

		char file_name[PATH_MAX];
		char append[100];
		//strcpy (file_name, address_index);
		sprintf(append,".fs_matrix_rank_%d.csv", i);
		//strcat(file_name, append);
		sprintf(file_name, "%s/%s-%d-%d-%s", output_directory, executable_name, pid, (*it).object_id, append );
		fp = fopen (file_name, "w+");
        	fprintf(fs_fp, "largest %d: object with id: %d, count: %0.2lf, from variable %s ", i, (*it).object_id, (*it).count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
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
	//fprintf(stderr, "reach 8\n");
	FILE * fs_core_fp;
	char fs_core_file_name[PATH_MAX];
	sprintf(fs_core_file_name, "%s/%s-%d-fs_core_object_ranking.txt", output_directory, executable_name, pid );
	fs_core_fp = fopen (fs_core_file_name, "w+");    
	//fprintf(stderr, "reach 9\n");
	for (it = object_fs_core_count.begin(), i = 0; it != object_fs_core_count.end() && i < 50; it++, i++) 
	{  
		adm_object_t* obj = adm_db_find_by_object_id((*it).object_id); 
		FILE * fp;

		char file_name[PATH_MAX];
		char append[100];
		//strcpy (file_name, address_index);
		sprintf(append,".fs_core_matrix_rank_%d.csv", i);
		//strcat(file_name, append);
		sprintf(file_name, "%s/%s-%d-%d-%s", output_directory, executable_name, pid, (*it).object_id, append );
		fp = fopen (file_name, "w+");
        	fprintf(fs_core_fp, "largest %d: object with id: %d, count: %0.2lf, from variable %s ", i, (*it).object_id, (*it).count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
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
	//fprintf(stderr, "reach 10\n");
	// before
	FILE * comm_fp;
	char comm_file_name[PATH_MAX];
	sprintf(comm_file_name, "%s/%s-%d-as_object_ranking.txt", output_directory, executable_name, pid );
	comm_fp = fopen (comm_file_name, "w+");    
	//fprintf(stderr, "reach 11\n");
	for (it = object_comm_count.begin(), i = 0; it != object_comm_count.end() && i < 50; it++, i++) 
	{  
		adm_object_t* obj = adm_db_find_by_object_id((*it).object_id); 
		FILE * fp;


		char file_name[PATH_MAX];
		char append[100];
		//strcpy (file_name, address_index);
		sprintf(append,".as_matrix_rank_%d.csv", i);
		//strcat(file_name, append);
		sprintf(file_name, "%s/%s-%d-%d-%s", output_directory, executable_name, pid, (*it).object_id, append );

		fp = fopen (file_name, "w+");
        	fprintf(comm_fp, "largest %d: object with id: %d, count: %0.2lf, from variable %s ", i, (*it).object_id, (*it).count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
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
	//fprintf(stderr, "reach 12\n");
	FILE * comm_core_fp;
	char comm_core_file_name[PATH_MAX];
	sprintf(comm_core_file_name, "%s/%s-%d-as_core_object_ranking.txt", output_directory, executable_name, pid );
	comm_core_fp = fopen (comm_core_file_name, "w+");    
	//fprintf(stderr, "reach 13\n");
	for (it = object_comm_core_count.begin(), i = 0; it != object_comm_core_count.end() && i < 50; it++, i++) 
	{  
		adm_object_t* obj = adm_db_find_by_object_id((*it).object_id); 
		FILE * fp;

		char file_name[PATH_MAX];
		char append[100];
		//strcpy (file_name, address_index);
		sprintf(append,".as_core_matrix_rank_%d.csv", i);
		//strcat(file_name, append);
		sprintf(file_name, "%s/%s-%d-%d-%s", output_directory, executable_name, pid, (*it).object_id, append );
		fp = fopen (file_name, "w+");
        	fprintf(comm_core_fp, "largest %d: object with id: %d, count: %0.2lf, from variable %s ", i, (*it).object_id, (*it).count, static_cast<char*>(obj->meta.meta[ADM_META_VAR_TYPE]));
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
	//fprintf(stderr, "reach 14\n");
	//fprintf(stderr, "pretty printing tree of objects\n");
	//object_tree->postorder(0);
	/*fprintf(stderr, "pretty printing tree of address ranges\n");
	tree->postorder(0);*/
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
  int a = get_object_id();
  adm_out(&a, sizeof(a));
  /*uint64_t z = get_size();
  adm_out(&z, sizeof(z));
  state_t s = get_state();
  adm_out(&s, sizeof(s));*/
  //adm_meta_print_stack
  //adm_meta_print_base

  meta.print();
}

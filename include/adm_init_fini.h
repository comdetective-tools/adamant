void adm_initialize();

void adm_finalize(int flag, char output_directory[], const char * executable_name, int pid);

//void inc_false_matrix(uint64_t address, int a, int b, double inc_false_matrix);

void inc_false_matrix(uint64_t address1, uint64_t address2, int a, int b, double inc_false_matrix);

void inc_true_matrix(uint64_t address, int a, int b, double inc_false_matrix);

void malloc_adm(void* ptr, size_t size, int object_id);

void free_adm(void* ptr);

void realloc_adm(void *ptr, size_t size, int object_id);

void posix_memalign_adm(int ret, void** memptr, size_t alignment, size_t size, int object_id);

void memalign_adm(void * ptr, size_t size, int object_id);

void aligned_alloc_adm(void * ptr, size_t size, int object_id);

void valloc_adm(void * ptr, size_t size, int object_id);

void pvalloc_adm(void * ptr, size_t size, int object_id);

void numa_alloc_onnode_adm(void* ptr, size_t size, int object_id);

void numa_alloc_interleaved_adm(void* ptr, size_t size, int object_id);

void mmap_adm(void* ptr, size_t size, int object_id);

void mmap64_adm(void* ptr, size_t size, int object_id);

char * adm_get_var_name(uint64_t address);

void inc_true_count(uint64_t address, double inc);

void inc_true_core_count(uint64_t address, double inc);

void inc_false_count(uint64_t address1, uint64_t address2, double inc);

void inc_false_core_count(uint64_t address1, uint64_t address2, double inc);

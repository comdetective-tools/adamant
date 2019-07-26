void adm_initialize();

void adm_finalize(int flag, char output_directory[], const char * executable_name, int pid);

//void inc_false_matrix(uint64_t address, int a, int b, double inc_false_matrix);

void inc_false_matrix(uint64_t address1, uint64_t address2, int a, int b, double inc_false_matrix);

void inc_true_matrix(uint64_t address, int a, int b, double inc_false_matrix);

void* malloc_adm(void* ptr, size_t size);

void free_adm(void* ptr);

void* calloc_adm(size_t nmemb, size_t size);

void* calloc_adm2(size_t nmemb, size_t size);
/*
void* realloc_adm(void *ptr, size_t size);

int posix_memalign_adm(void** memptr, size_t alignment, size_t size);

void* memalign_adm(size_t alignment, size_t size);

void* aligned_alloc_adm(size_t alignment, size_t size);

void* valloc_adm(size_t size);

void* pvalloc_adm(size_t size);*/

void realloc_adm(void * pptr, void *ptr, size_t size);

int posix_memalign_adm(int ret, void** memptr, size_t alignment, size_t size);

void memalign_adm(void * ptr, size_t size);

void aligned_alloc_adm(void * ptr, size_t size);

void valloc_adm(void * ptr, size_t size);

void pvalloc_adm(void * ptr, size_t size);

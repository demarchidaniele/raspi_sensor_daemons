int shmem_server_init(key_t key, size_t size, void** ptr);
void shmem_client_init(key_t key, size_t size, void** ptr);
int shmem_detach(void** ptr, int id);

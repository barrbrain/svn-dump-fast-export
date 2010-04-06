#ifndef REPO_TREE_H_
#define REPO_TREE_H_

#define REPO_MODE_DIR 0040000
#define REPO_MODE_BLB 0100644
#define REPO_MODE_EXE 0100755
#define REPO_MODE_LNK 0120000

#define REPO_MAX_PATH_LEN 4096

uint32_t repo_copy(uint32_t revision, char *src, char *dst);

void repo_add(char *path, uint32_t mode, uint32_t blob_mark);

uint32_t repo_replace(char *path, uint32_t blob_mark);

void repo_modify(char *path, uint32_t mode, uint32_t blob_mark);

void repo_delete(char *path);

void repo_commit(uint32_t revision, char * author, char * log, char * uuid,
                 char * url, time_t timestamp);

void repo_copy_blob(uint32_t mode, uint32_t mark, uint32_t len);

#endif

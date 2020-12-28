#ifndef PUBLIC_MEM_H
#define PUBLIC_MEM_H

#include <stdio.h>
#include <stdlib.h>

typedef union {
    long        l_dummy;
    double      d_dummy;
    void        *p_dummy;
} Cell;

typedef struct MemoryPage_tag *MemoryPageList;

/* 页链表 */
typedef struct MemoryPage_tag {
    int                 total_cell_num;
    int                 use_cell_num;
    MemoryPageList      next;
    Cell                cell[1];
}MemoryPage;

/* MEM storage数据结构，包含当前页和页链表 */
typedef struct MEM_Storage_tag {
    MemoryPageList      page_list;
    int                 current_page_size;
}*MEM_Storage;


#define MEM_malloc(size) (MEM_malloc_func(__FILE__, __LINE__, size))
#define MEM_realloc(ptr, size) (MEM_realloc_func(__FILE__, __LINE__, ptr, size))
#define MEM_strdup(str) (MEM_strdup_func(__FILE__, __LINE__, str))

#define MEM_open_storage(page_size) (MEM_open_storage_func(__FILE__, __LINE__, page_size))
#define MEM_storage_malloc(storage, size) (MEM_storage_malloc_func(__FILE__, __LINE__, storage, size))

/* memory.c */
void *MEM_malloc_func(char *filename, int line, size_t size);
void *MEM_realloc_func(char *filename, int line, void *ptr, size_t size);
char *MEM_strdup_func(char *filename, int line, char *str);
MEM_Storage MEM_open_storage_func(char *filename, int line, int page_size);
void *MEM_storage_malloc_func(char *filename, int line, MEM_Storage storage, size_t size);
void MEM_free(void *ptr);
void MEM_dispose_storage(MEM_Storage storage);

#endif  /* PUBLIC_MEM_H */


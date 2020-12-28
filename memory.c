#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "MEM.h"

#define CELL_SIZE               (sizeof(Cell))
#define DEFAULT_PAGE_SIZE       (1024)  /* cell num */

/* 开辟空间 */
MEM_Storage MEM_open_storage_func(char *filename, int line, int page_size)
{
    MEM_Storage storage = MEM_malloc_func(filename, line, sizeof(struct MEM_Storage_tag));
    storage->page_list = NULL;
    assert(page_size >= 0);
    /* page_size>0则设置为传入的page_size，否则设置为默认值 */
    if (page_size > 0) {
        storage->current_page_size = page_size;
    } else {
        storage->current_page_size = DEFAULT_PAGE_SIZE;
    }

    return storage;
}

/* 分配内存 */
void* MEM_storage_malloc_func(char *filename, int line, MEM_Storage storage, size_t size)
{
    int cell_num = ((size - 1) / CELL_SIZE) + 1;    /* 计算分配的cell数量 */
    MemoryPage *new_page;
    void *p;
    
    /* 页表非空且cell使用数量少于限定数量 */
    if (storage->page_list != NULL
        && (storage->page_list->use_cell_num + cell_num < storage->page_list->total_cell_num)) {
        p = &(storage->page_list->cell[storage->page_list->use_cell_num]);
        storage->page_list->use_cell_num += cell_num;
    } 
    /* 页表空或使用数量已超限，新建页 */
    else {
        /* 计算所需分配cell数量，在cellnum和pagesize中取较大值 */
        int alloc_cell_num = (cell_num > storage->current_page_size) ?
                            cell_num : storage->current_page_size;
        new_page = MEM_malloc_func(filename, line, sizeof(MemoryPage)
                                   + CELL_SIZE * (alloc_cell_num - 1));
        
        /* 头插法加入现有页表 */
        new_page->next = storage->page_list;
        new_page->total_cell_num = alloc_cell_num;
        new_page->use_cell_num = cell_num;
        storage->page_list = new_page;
        p = &(new_page->cell[0]);

    }

    return p;
}

/* 清除storage */
void MEM_dispose_storage(MEM_Storage storage)
{
    MemoryPage  *temp;
    /* 逐一释放页表，最后释放整个storage */
    while (storage->page_list) {
        temp = storage->page_list->next;
        MEM_free(storage->page_list);
        storage->page_list = temp;
    }
    MEM_free(storage);
}


/* 处理错误函数 */
static void error_handler(char *filename, int line, char *msg)
{
    /* 打印错误信息 */
    fprintf(stderr, "MEM:%s failed in %s at %d\n", msg, filename, line);
    exit(1);
}


/* 分配内存空间 */
void* MEM_malloc_func(char *filename, int line, size_t size)
{
    size_t alloc_size = size;
    void *ptr = malloc(alloc_size);     /* 使用malloc返回指定大小的内存指针 */
    if (ptr == NULL) {
        error_handler(filename, line, "malloc");
    }
    return ptr;
}

/* realloc函数 */
void* MEM_realloc_func(char *filename, int line, void *ptr, size_t size)
{
    size_t  alloc_size = size;
    void *real_ptr = ptr;
    void *new_ptr = realloc(real_ptr, alloc_size);

    if (new_ptr == NULL) {
        if (ptr == NULL) {
            error_handler(filename, line, "realloc(malloc)");
        } else {
            error_handler(filename, line, "realloc");
            free(real_ptr);
        }
    }
    return(new_ptr);
}

/* 传入字串常量，返回字串指针 */
char * MEM_strdup_func(char *filename, int line, char *str)
{
    size_t alloc_size = strlen(str) + 1;;
    char *ptr = malloc(alloc_size);
    if (ptr == NULL) {
        error_handler(filename, line, "strdup");
    }
    strcpy(ptr, str);
    return(ptr);
}

/* 释放指针内存 */
void MEM_free(void *ptr)
{
    if (ptr == NULL)
        return;
    void *real_ptr = ptr;
    free(real_ptr);
}


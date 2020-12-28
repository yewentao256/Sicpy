#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include "MEM.h"
#include "DBG.h"

static char     *st_current_file_name;
static int      st_current_line;
static char     *st_assert_expression;


/* 断言函数接口，接收变长参数 */
void DBG_assert_func(char *message, ...)
{
    va_list     ap;
    va_start(ap, message);
    fprintf(stderr, "Assertion failure (%s)   file:%s   line:%d\n",
        st_assert_expression, st_current_file_name, st_current_line);
    if(message){
        vfprintf(stderr, message, ap);
    }
    va_end(ap);
    abort();
}


/* panic函数接口，接收变长参数 */
void DBG_panic_func(char *message, ...)
{
    va_list     ap;
    va_start(ap, message);
    fprintf(stderr, "Panic!! file:%s line:%d\n", st_current_file_name, st_current_line);
    if(message){
        vfprintf(stderr, message, ap);
    }
    va_end(ap);
    abort();
}


/* 设置当前file和当前line */
void DBG_set(char *file, int line)
{
    st_current_file_name = file;
    st_current_line = line;
}

/* 设置断言表达式 */
void DBG_set_expression(char *expression)
{
    st_assert_expression = expression;
}

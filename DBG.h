#ifndef PUBLIC_DBG_H_INCLUDED
#define PUBLIC_DBG_H_INCLUDED
#include <stdio.h>      
#include <stdarg.h>

/* 对外assert接口，如果传入表达式为真则什么都不做，否则进行设置后调用DBG_assert_func */
#define DBG_assert(expression, args) \
 ((expression) ? (void)(0) : ((DBG_set(__FILE__, __LINE__)),\
   (DBG_set_expression(#expression)), DBG_assert_func args))
  
/* 对外panic接口，进行设置后调用DBG_panic_func */
#define DBG_panic(args) ((DBG_set(__FILE__, __LINE__)), DBG_panic_func args)



/* debug.c，下列函数请不要直接调用 */
void DBG_set(char *file, int line);
void DBG_set_expression(char *expression);
void DBG_assert_func(char *message, ...);
void DBG_panic_func(char *message, ...);
#endif /* PUBLIC_DBG_H_INCLUDED */

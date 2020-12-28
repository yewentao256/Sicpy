#include <stdio.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "sicpy.h"

/* 分配SCP字串空间 */
SCP_String * alloc_scp_string(char *str, SCP_Boolean is_literal)
{
    SCP_String *scp_string = MEM_malloc(sizeof(SCP_String));
    scp_string->ref_count = 0;
    scp_string->is_literal = is_literal;
    scp_string->string = str;
    return scp_string;
}

/* 释放字串 */
void scp_release_string(SCP_String *str)
{
    str->ref_count--;

    /* 断言引用计数至少应比0大 */
    DBG_assert(str->ref_count >= 0, ("str->ref_count..%d\n", str->ref_count));
    if (str->ref_count == 0) {
        /* 如果不是字面常量，先释放字符数组，再释放整个字符串结构体 */
        if (!str->is_literal) {
            MEM_free(str->string);
        }
        MEM_free(str);
    }
}

/* 创建SCP字符串 */
SCP_String * scp_create_sicpy_string(char *str)
{
    SCP_String *scp_string = alloc_scp_string(str, SCP_FALSE);
    scp_string->ref_count = 1;

    return scp_string;
}

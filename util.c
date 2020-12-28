#include <stdio.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "sicpy.h"

static SCP_Interpreter *st_interpreter;

/* 获取当前解释器*/
SCP_Interpreter * scp_get_interpreter(void)
{
    return st_interpreter;
}

/* 设置当前解释器 */
void scp_set_current_interpreter(SCP_Interpreter *inter)
{
    st_interpreter = inter;
}

/* 查询函数定义 */
FunctionDefinition * scp_search_function(char *name)
{
    FunctionDefinition *func;
    SCP_Interpreter *inter = scp_get_interpreter();

    for (func = inter->function_list; func; func = func->next) {
        /* strcmp比较的两字符串，如果相等则返回0，str1<str2则返回负数，反之返回整数 */
        if (!strcmp(func->name, name))
            break;
    }
    return func;
}


/* 分配内存 */
void * scp_malloc(size_t size)
{
    SCP_Interpreter *inter = scp_get_interpreter();
    void *p = MEM_storage_malloc(inter->interpreter_storage, size);

    return p;
}



/* 搜索局部变量 */
Variable * scp_search_local_variable(LocalEnvironment *env, char *identifier)
{
    Variable    *pos;

    if (env == NULL)
        return NULL;
    for (pos = env->variable; pos; pos = pos->next) {
        if (!strcmp(pos->name, identifier))
            break;
    }
    if (pos == NULL) {
        return NULL;
    } else {
        return pos;
    }
}

/* 遍历链表，搜索全局变量 */
Variable * scp_search_global_variable(SCP_Interpreter *inter, char *identifier)
{
    Variable    *pos;
    for (pos = inter->variable; pos; pos = pos->next) {
        if (!strcmp(pos->name, identifier))
            return pos;
    }
    return NULL;
}

/* 添加局部变量 */
void scp_add_local_variable(LocalEnvironment *env, char *identifier, SCP_Value *value)
{
    Variable    *new_variable = MEM_malloc(sizeof(Variable));
    new_variable->name = identifier;
    new_variable->value = *value;
    /* 头插法加入局部环境中 */
    new_variable->next = env->variable;
    env->variable = new_variable;
}

/* 添加全局变量 */
void scp_add_global_variable(SCP_Interpreter *inter, char *identifier, SCP_Value *value)
{
    Variable *new_variable = MEM_storage_malloc(inter->execute_storage, sizeof(Variable));
    /* 分配内存并将标识符拷贝至新变量的name处 */
    new_variable->name = MEM_storage_malloc(inter->execute_storage, strlen(identifier) + 1);
    strcpy(new_variable->name, identifier);
    new_variable->value = *value;
    /* 头插法加入全局环境中 */
    new_variable->next = inter->variable;
    inter->variable = new_variable;
}

/* 获取当前操作符的字串，如传入ASSIGN_EXPRESSION返回= */
char * scp_get_operator_string(ExpressionType type)
{
    char        *str;
    switch (type) {
    case ASSIGN_EXPRESSION:
        str = "=";
        break;
    case ADD_EXPRESSION:
        str = "+";
        break;
    case SUB_EXPRESSION:
        str = "-";
        break;
    case MUL_EXPRESSION:
        str = "*";
        break;
    case DIV_EXPRESSION:
        str = "/";
        break;
    case MOD_EXPRESSION:
        str = "%";
        break;
    case LOGICAL_AND_EXPRESSION:
        str = "&&";
        break;
    case LOGICAL_OR_EXPRESSION:
        str = "||";
        break;
    case EQ_EXPRESSION:
        str = "==";
        break;
    case NE_EXPRESSION:
        str = "!=";
        break;
    case GT_EXPRESSION:
        str = "<";
        break;
    case GE_EXPRESSION:
        str = "<=";
        break;
    case LT_EXPRESSION:
        str = ">";
        break;
    case LE_EXPRESSION:
        str = ">=";
        break;
    case MINUS_EXPRESSION:
        str = "-";
        break;
    case FUNCTION_CALL_EXPRESSION:
    case NULL_EXPRESSION:
    case EXPRESSION_TYPE_COUNT_PLUS_1:
    case BOOLEAN_EXPRESSION:
    case INT_EXPRESSION:
    case DOUBLE_EXPRESSION:
    case STRING_EXPRESSION:
    case IDENTIFIER_EXPRESSION:
    default:
        DBG_panic(("bad expression type..%d\n", type));
    }

    return str;
}

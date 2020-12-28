#include <math.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "sicpy.h"


/* 如果是字符串，引用计数+1 */
static void add_refer_if_string(SCP_Value *v)
{
    if (v->type == SCP_STRING_VALUE) {
        v->u.string_value->ref_count++;
    }
}

/* 如果是字符串则进行释放 */
static void release_if_string(SCP_Value *v)
{
    if (v->type == SCP_STRING_VALUE) {
        scp_release_string(v->u.string_value);
    }
}

/* 寻找全局变量 */
static Variable * search_global_variable_from_env(SCP_Interpreter *inter,
                                LocalEnvironment *env, char *name)
{
    GlobalVariableRef *pos;
    /* 如果局部环境为空，直接搜索全局变量 */
    if (env == NULL) {
        return scp_search_global_variable(inter, name);
    }

    /* 否则搜索在局部变量中搜索 */
    for (pos = env->global_variable; pos; pos = pos->next) {
        if (!strcmp(pos->variable->name, name)) {
            return pos->variable;
        }
    }

    return NULL;
}

/* 获取标识符的值 */
static SCP_Value get_identifier_value(SCP_Interpreter *inter,
                           LocalEnvironment *env, Expression *expr)
{
    SCP_Value   v;
    /* 搜索局部变量 */
    Variable    *vp = scp_search_local_variable(env, expr->u.identifier);
    if (vp != NULL) {
        v = vp->value;
    }
    /* 如果在局部变量中搜索不到则搜索全局变量 */
    else {
        vp = search_global_variable_from_env(inter, env, expr->u.identifier);
        if (vp != NULL) {
            v = vp->value;
        } else {
            scp_runtime_error(expr->line_number, VARIABLE_NOT_FOUND_ERR, STRING_MESSAGE_ARGUMENT,
                              "name", expr->u.identifier, MESSAGE_ARGUMENT_END);
        }
    }
    /* 如果是字符串则引用计数+1*/
    add_refer_if_string(&v);
    return v;
}

static SCP_Value eval_expression(SCP_Interpreter *inter, LocalEnvironment *env, Expression *expr);

/* 获取赋值表达式的值 */
static SCP_Value eval_assign_expression(SCP_Interpreter *inter, LocalEnvironment *env,
                       char *identifier, Expression *expression)
{
    SCP_Value v = eval_expression(inter, env, expression);
    Variable *left = scp_search_local_variable(env, identifier);

    if (left == NULL) {
        left = search_global_variable_from_env(inter, env, identifier);
    }
    /* 在全局变量中找到了该标识符 */
    if (left != NULL) {
        /* 如果左边identifier原来代表字符串，则释放并减少计数引用 */
        release_if_string(&left->value);
        left->value = v;
        add_refer_if_string(&v);
    }
    /* 找不到标识符 */
    else {
        /* 如果局部环境存在，变量新建至局部环境，否则新建至全局环境 */
        if (env != NULL) {
            scp_add_local_variable(env, identifier, &v);
        } else {
            scp_add_global_variable(inter, identifier, &v);
        }
        add_refer_if_string(&v);
    }
    return v;
}

/* 左右均为bool值的运算 */
static SCP_Boolean eval_binary_boolean(SCP_Interpreter *inter, ExpressionType operator,
                    SCP_Boolean left, SCP_Boolean right, int line_number)
{
    SCP_Boolean result;

    if (operator == EQ_EXPRESSION) {
        result = left == right;
    }
    else if (operator == NE_EXPRESSION) {
        result = left != right;
    }
    else {
        char *op_str = scp_get_operator_string(operator);   /* 获取当前操作符的字串，如* */
        scp_runtime_error(line_number, NOT_BOOLEAN_OPERATOR_ERR,
                          STRING_MESSAGE_ARGUMENT, "operator", op_str, MESSAGE_ARGUMENT_END);
    }
    return result;
}

/* 表达式左右都为int类型的处理 */
static void eval_binary_int(SCP_Interpreter *inter, ExpressionType operator,
                int left, int right, SCP_Value *result, int line_number)
{
    /* 判断结果类型 */
    if (dkc_is_math_operator(operator)) {
        result->type = SCP_INT_VALUE;
    } else if (dkc_is_compare_operator(operator)) {
        result->type = SCP_BOOLEAN_VALUE;
    } else {
        DBG_panic(("operator..%d\n", operator));
    }

    switch (operator) {    
    /* 常规数学运算 */
    case ADD_EXPRESSION:
        result->u.int_value = left + right;
        break;
    case SUB_EXPRESSION:
        result->u.int_value = left - right;
        break;
    case MUL_EXPRESSION:
        result->u.int_value = left * right;
        break;
    case DIV_EXPRESSION:
        result->u.int_value = left / right;
        break;
    case MOD_EXPRESSION:
        result->u.int_value = left % right;
        break;
    /* 常规逻辑运算 */
    case EQ_EXPRESSION:
        result->u.boolean_value = left == right;
        break;
    case NE_EXPRESSION:
        result->u.boolean_value = left != right;
        break;
    case GT_EXPRESSION:
        result->u.boolean_value = left > right;
        break;
    case GE_EXPRESSION:
        result->u.boolean_value = left >= right;
        break;
    case LT_EXPRESSION:
        result->u.boolean_value = left < right;
        break;
    case LE_EXPRESSION:
        result->u.boolean_value = left <= right;
        break;
    /* 以下情况全部不应该出现 */
    case LOGICAL_AND_EXPRESSION:            /* 不支持直接对int进行逻辑与操作 */
    case LOGICAL_OR_EXPRESSION:             /* 不支持直接对int进行逻辑或操作 */
    case BOOLEAN_EXPRESSION:
    case INT_EXPRESSION:
    case DOUBLE_EXPRESSION:
    case STRING_EXPRESSION:
    case IDENTIFIER_EXPRESSION:
    case ASSIGN_EXPRESSION:
    case MINUS_EXPRESSION:
    case FUNCTION_CALL_EXPRESSION:
    case NULL_EXPRESSION:
    case EXPRESSION_TYPE_COUNT_PLUS_1:
    default:
        DBG_panic(("bad case...%d", operator));
    }
}

/* 含有double的计算 */
static void eval_binary_double(ExpressionType operator,
                   double left, double right, SCP_Value *result, int line_number)
{   
    /* 判断结果类型 */
    if (dkc_is_math_operator(operator)) {
        result->type = SCP_DOUBLE_VALUE;
    } else if (dkc_is_compare_operator(operator)) {
        result->type = SCP_BOOLEAN_VALUE;
    } else {
        DBG_panic(("operator..%d\n", operator));
    }

    /* switch分情况处理操作符 */
    switch (operator) {
    /* 数学运算符 */
    case ADD_EXPRESSION:
        result->u.double_value = left + right;
        break;
    case SUB_EXPRESSION:
        result->u.double_value = left - right;
        break;
    case MUL_EXPRESSION:
        result->u.double_value = left * right;
        break;
    case DIV_EXPRESSION:
        if (right==0) {
            scp_runtime_error(line_number, DIVISION_BY_ZERO_ERR,
                          STRING_MESSAGE_ARGUMENT, "operator", operator, MESSAGE_ARGUMENT_END);
        }
        result->u.double_value = left / right;
        break;
    case MOD_EXPRESSION:
        result->u.double_value = fmod(left, right);
        break;

    /* 逻辑运算符 */
    case EQ_EXPRESSION:
        result->u.int_value = left == right;
        break;
    case NE_EXPRESSION:
        result->u.int_value = left != right;
        break;
    case GT_EXPRESSION:
        result->u.int_value = left > right;
        break;
    case GE_EXPRESSION:
        result->u.int_value = left >= right;
        break;
    case LT_EXPRESSION:
        result->u.int_value = left < right;
        break;
    case LE_EXPRESSION:
        result->u.int_value = left <= right;
        break;
    
    /* 以下是不应该出现的情况 */
    case MINUS_EXPRESSION:
    case FUNCTION_CALL_EXPRESSION:
    case NULL_EXPRESSION:
    case EXPRESSION_TYPE_COUNT_PLUS_1:
    case BOOLEAN_EXPRESSION:
    case INT_EXPRESSION:
    case DOUBLE_EXPRESSION:
    case STRING_EXPRESSION:
    case IDENTIFIER_EXPRESSION:
    case ASSIGN_EXPRESSION:
    case LOGICAL_AND_EXPRESSION:
    case LOGICAL_OR_EXPRESSION:
    default:
        DBG_panic(("bad default...%d", operator));
    }
}

/* 比较字符串 */
static SCP_Boolean eval_compare_string(ExpressionType operator,
                    SCP_Value *left, SCP_Value *right, int line_number)
{
    SCP_Boolean result;
    int cmp = strcmp(left->u.string_value->string, right->u.string_value->string);

    /* 根据操作符及cmp返回值返回结果*/
    if (operator == EQ_EXPRESSION) {
        result = (cmp == 0);
    }
    else if (operator == NE_EXPRESSION) {
        result = (cmp != 0);
    }
    else if (operator == GT_EXPRESSION) {
        result = (cmp > 0);
    }
    else if (operator == GE_EXPRESSION) {
        result = (cmp >= 0);
    }
    else if (operator == LT_EXPRESSION) {
        result = (cmp < 0);
    }
    else if (operator == LE_EXPRESSION) {
        result = (cmp <= 0);
    }
    /* 非上述任一种操作符，报错 */
    else {
        char *op_str = scp_get_operator_string(operator);
        scp_runtime_error(line_number, BAD_OPERATOR_FOR_STRING_ERR,
                          STRING_MESSAGE_ARGUMENT, "operator", op_str, MESSAGE_ARGUMENT_END);
    }
    scp_release_string(left->u.string_value);
    scp_release_string(right->u.string_value);

    return result;
}

/* 计算有NULL的表达式 */
static SCP_Boolean eval_binary_null(ExpressionType operator,
                 SCP_Value *left, SCP_Value *right, int line_number)
{
    SCP_Boolean result;

    /* 如果是==操作符，NULL==NULL也应该为真 */
    if (operator == EQ_EXPRESSION) {
        result = left->type == SCP_NULL_VALUE && right->type == SCP_NULL_VALUE;
    }
    /* 如果是!=操作符，a!=NULL应该为真 */
    else if (operator == NE_EXPRESSION) {
        result = !(left->type == SCP_NULL_VALUE && right->type == SCP_NULL_VALUE);
    }
    /* 否则报错 */
    else {
        char *op_str = scp_get_operator_string(operator);
        scp_runtime_error(line_number, NOT_NULL_OPERATOR_ERR,
                          STRING_MESSAGE_ARGUMENT, "operator", op_str, MESSAGE_ARGUMENT_END);
    }
    release_if_string(left);
    release_if_string(right);

    return result;
}

/* 连接字符串 */
SCP_String * chain_string(SCP_Interpreter *inter, SCP_String *left, SCP_String *right)
{
    int len = strlen(left->string) + strlen(right->string);
    char *str = MEM_malloc(len + 1);

    /* 复制左边字串，连接右边字串 */
    strcpy(str, left->string);
    strcat(str, right->string);

    SCP_String *ret = scp_create_sicpy_string(str);
    scp_release_string(left);
    scp_release_string(right);
    return ret;
}

/* 计算表达式，传入解释器和当前环境、操作符和左右表达式进行运算 */
SCP_Value scp_eval_binary_expression(SCP_Interpreter *inter, LocalEnvironment *env,
                           ExpressionType operator, Expression *left, Expression *right)
{
    SCP_Value   left_val = eval_expression(inter, env, left);
    SCP_Value   right_val = eval_expression(inter, env, right);
    SCP_Value   result;

    /* 左右都为int类型的计算 */
    if (left_val.type == SCP_INT_VALUE && right_val.type == SCP_INT_VALUE) {
        eval_binary_int(inter, operator, left_val.u.int_value,
                        right_val.u.int_value, &result, left->line_number);
    }
    /* 左右都为double类型的计算 */
    else if (left_val.type == SCP_DOUBLE_VALUE && right_val.type == SCP_DOUBLE_VALUE) {
        eval_binary_double(operator, left_val.u.double_value,
                            right_val.u.double_value, &result, left->line_number);

    }
    /* 左边int右边double类型的计算 */
    else if (left_val.type == SCP_INT_VALUE && right_val.type == SCP_DOUBLE_VALUE) {
        left_val.u.double_value = left_val.u.int_value;     /* 类型转换 */
        eval_binary_double(operator, left_val.u.double_value, right_val.u.double_value,
                           &result, left->line_number);
    }
    /* 左边double右边int类型的计算 */
    else if (left_val.type == SCP_DOUBLE_VALUE && right_val.type == SCP_INT_VALUE) {
        right_val.u.double_value = right_val.u.int_value;
        eval_binary_double(operator, left_val.u.double_value, right_val.u.double_value,
                           &result, left->line_number);
    }
    /* 左右均为bool值的计算 */
    else if (left_val.type == SCP_BOOLEAN_VALUE && right_val.type == SCP_BOOLEAN_VALUE) {
        result.type = SCP_BOOLEAN_VALUE;
        result.u.boolean_value = eval_binary_boolean(inter, operator, left_val.u.boolean_value,
                                  right_val.u.boolean_value, left->line_number);
    }
    /* 左边字符串且操作符为加的处理 */
    else if (left_val.type == SCP_STRING_VALUE && operator == ADD_EXPRESSION) {
        char    buf[LINE_BUF_SIZE];
        SCP_String *right_str;

        /* 右边为int值 */
        if (right_val.type == SCP_INT_VALUE) {
            sprintf(buf, "%d", right_val.u.int_value);
            right_str = scp_create_sicpy_string(MEM_strdup(buf));
        }
        /* 右边为double */
        else if (right_val.type == SCP_DOUBLE_VALUE) {
            sprintf(buf, "%f", right_val.u.double_value);
            right_str = scp_create_sicpy_string(MEM_strdup(buf));
        }
        /* 右边为布尔值，将布尔值处理为true或false字符串 */
        else if (right_val.type == SCP_BOOLEAN_VALUE) {
            if (right_val.u.boolean_value) {
                right_str = scp_create_sicpy_string(MEM_strdup("true"));
            } else {
                right_str = scp_create_sicpy_string(MEM_strdup("false"));
            }
        }
        /* 右边为字符串 */
        else if (right_val.type == SCP_STRING_VALUE) {
            right_str = right_val.u.string_value;
        }
        /* 右边为指针 */
        else if (right_val.type == SCP_NATIVE_POINTER_VALUE) {
            sprintf(buf, "(%s:%p)",
                    right_val.u.native_pointer.info, right_val.u.native_pointer.pointer);
            right_str = scp_create_sicpy_string(MEM_strdup(buf));
        } 
        /* 右边为空 */
        else if (right_val.type == SCP_NULL_VALUE) {
            right_str = scp_create_sicpy_string(MEM_strdup("null"));
        } 
        result.type = SCP_STRING_VALUE;
        result.u.string_value = chain_string(inter, left_val.u.string_value, right_str);

    }
    
    /* 如果左右两边都是字符串且操作符不为+ */
    else if (left_val.type == SCP_STRING_VALUE && right_val.type == SCP_STRING_VALUE) {
        result.type = SCP_BOOLEAN_VALUE;
        result.u.boolean_value = eval_compare_string(operator, &left_val, &right_val,
                                                    left->line_number);
    } 
    /* 如果有任一边为NULL */
    else if (left_val.type == SCP_NULL_VALUE || right_val.type == SCP_NULL_VALUE) {
        result.type = SCP_BOOLEAN_VALUE;
        result.u.boolean_value = eval_binary_null(operator, &left_val, &right_val, left->line_number);
    } 
    /* 其他情况则报错 */
    else {
        char *op_str = scp_get_operator_string(operator);
        scp_runtime_error(left->line_number, BAD_OPERAND_TYPE_ERR,
                          STRING_MESSAGE_ARGUMENT, "operator", op_str, MESSAGE_ARGUMENT_END);
    }

    return result;
}

/* 逻辑与或计算 */
static SCP_Value eval_logical_and_or_expression(SCP_Interpreter *inter, LocalEnvironment *env,
                               ExpressionType operator,Expression *left, Expression *right)
{   
    SCP_Value   left_val = eval_expression(inter, env, left);    /* 先计算左侧值 */
    SCP_Value   right_val;
    SCP_Value   result;
    result.type = SCP_BOOLEAN_VALUE;

    /* 左侧计算好的值需要是bool值，否则报错 */
    if (left_val.type != SCP_BOOLEAN_VALUE) {
        scp_runtime_error(left->line_number, NOT_BOOLEAN_TYPE_ERR, MESSAGE_ARGUMENT_END);
    }
    /* 操作符为逻辑与且左侧为假，短路 */
    if (operator == LOGICAL_AND_EXPRESSION) {
        if (!left_val.u.boolean_value) {
            result.u.boolean_value = SCP_FALSE;
            return result;
        }
    } 
    /* 操作符为逻辑或且左侧为真，短路 */
    else if (operator == LOGICAL_OR_EXPRESSION) {
        if (left_val.u.boolean_value) {
            result.u.boolean_value = SCP_TRUE;
            return result;
        }
    } 
    else {
        DBG_panic(("bad operator..%d\n", operator));
    }

    right_val = eval_expression(inter, env, right);
    if (right_val.type != SCP_BOOLEAN_VALUE) {
        scp_runtime_error(right->line_number, NOT_BOOLEAN_TYPE_ERR, MESSAGE_ARGUMENT_END);
    }
    /* 经过短路判断之后，不管是或还是与，结果值即为右侧值 */
    result.u.boolean_value = right_val.u.boolean_value;

    return result;
}


/* 计算带负号的表达式 */
SCP_Value scp_eval_minus_expression(SCP_Interpreter *inter, LocalEnvironment *env,
                          Expression *exp)
{
    SCP_Value   exp_val = eval_expression(inter, env, exp);
    SCP_Value   result;
    /* 如果求值后为int类型 */
    if (exp_val.type == SCP_INT_VALUE) {
        result.type = SCP_INT_VALUE;
        result.u.int_value = -exp_val.u.int_value;
    }
    /* 如果求值后未double类型 */
    else if (exp_val.type == SCP_DOUBLE_VALUE) {
        result.type = SCP_DOUBLE_VALUE;
        result.u.double_value = -exp_val.u.double_value;
    }
    else {
        scp_runtime_error(exp->line_number, MINUS_OPERAND_TYPE_ERR,MESSAGE_ARGUMENT_END);
    }
    return result;
}


/* 清除局部环境 */
static void dispose_local_environment(LocalEnvironment *env)
{   
    /* 从头部开始释放变量链表 */
    while (env->variable) {
        Variable *temp = env->variable;
        if (env->variable->value.type == SCP_STRING_VALUE) {
            scp_release_string(env->variable->value.u.string_value);
        }
        env->variable = temp->next;
        MEM_free(temp);
    }
    /* 从头部开始释放全局变量链表 */
    while (env->global_variable) {
        GlobalVariableRef *ref = env->global_variable;
        env->global_variable = ref->next;
        MEM_free(ref);
    }
    MEM_free(env);
}

/* 调用原生函数 */
static SCP_Value call_native_function(SCP_Interpreter *inter, LocalEnvironment *env,
                     Expression *expr, SCP_NativeFunctionProc *proc)
{
    ArgumentList *arg_p;
    int arg_count = 0, i = 0;
    
    /* 实参计数 */
    for (arg_p = expr->u.function_call_expression.argument; arg_p; arg_p = arg_p->next) {
        arg_count++;
    }
    SCP_Value *args = MEM_malloc(sizeof(SCP_Value) * arg_count);
    /* 计算每个实参 */
    for (arg_p = expr->u.function_call_expression.argument; arg_p; arg_p = arg_p->next, i++){
        args[i] = eval_expression(inter, env, arg_p->expression);
    }
    /* 执行传入的原生函数 */
    SCP_Value value = proc(inter, arg_count, args);
    for (i = 0; i < arg_count; i++) {
        release_if_string(&args[i]);        /* 释放字串 */
    }
    MEM_free(args);
    return value;
}

/* 调用sicpy函数 */
static SCP_Value call_sicpy_function(SCP_Interpreter *inter, LocalEnvironment *env,
                      Expression *expr, FunctionDefinition *func)
{
    SCP_Value   value;
    ArgumentList        *arg_p;
    ParameterList       *param_p;

    /* 初始化局部环境 */
    LocalEnvironment    *local_env = MEM_malloc(sizeof(LocalEnvironment));
    local_env->variable = NULL;
    local_env->global_variable = NULL;

    for(arg_p = expr->u.function_call_expression.argument, param_p = func->u.sicpy_f.parameter;
        arg_p; arg_p = arg_p->next, param_p = param_p->next) {
        
        /* 如果实参还没传完而形参已传完，报错 */
        if (param_p == NULL) {
            scp_runtime_error(expr->line_number, ARGUMENT_TOO_MANY_ERR, MESSAGE_ARGUMENT_END);
        }
        /* 计算实参并添加至局部变量 */
        SCP_Value arg_val = eval_expression(inter, env, arg_p->expression);
        scp_add_local_variable(local_env, param_p->name, &arg_val);
    }
    /* 如果实参传完而形参还有剩余，报错 */
    if (param_p) {
        scp_runtime_error(expr->line_number, ARGUMENT_TOO_FEW_ERR, MESSAGE_ARGUMENT_END);
    }
    StatementResult result = scp_execute_statement_list(inter, local_env,
                                        func->u.sicpy_f.block->statement_list);
    /* 如果是正常的return结果，存入value中，否则置空 */
    if (result.type == RETURN_STATEMENT_RESULT) {
        value = result.return_value;
    } else {
        value.type = SCP_NULL_VALUE;
    }
    dispose_local_environment(local_env);

    return value;
}

/* 函数调用表达式计算 */
static SCP_Value eval_function_call_expression(SCP_Interpreter *inter, LocalEnvironment *env,
                              Expression *expr)
{
    SCP_Value  value;
    char *identifier = expr->u.function_call_expression.identifier;

    FunctionDefinition  *func = scp_search_function(identifier);
    /* 如果找不到该函数定义，报错 */
    if (func == NULL) {
        scp_runtime_error(expr->line_number, FUNCTION_NOT_FOUND_ERR,
                          STRING_MESSAGE_ARGUMENT, "name", identifier, MESSAGE_ARGUMENT_END);
    }
    switch (func->type) {
    /* sicpy函数定义类型 */
    case SICPY_FUNCTION_DEFINITION:
        value = call_sicpy_function(inter, env, expr, func);
        break;
    /* C函数定义类型 */
    case NATIVE_FUNCTION_DEFINITION:
        value = call_native_function(inter, env, expr, func->u.native_f.proc);
        break;
    default:
        DBG_panic(("bad case..%d\n", func->type));
    }

    return value;
}

/* 计算表达式 */
static SCP_Value eval_expression(SCP_Interpreter *inter, LocalEnvironment *env, Expression *expr)
{
    SCP_Value   v;

    /* 根据表达式类型计算 */
    switch (expr->type){
    case BOOLEAN_EXPRESSION:
        v.type = SCP_BOOLEAN_VALUE;
        v.u.boolean_value = expr->u.boolean_value;
        break;
    case INT_EXPRESSION:
        v.type = SCP_INT_VALUE;
        v.u.int_value = expr->u.int_value;
        break;
    case DOUBLE_EXPRESSION:
        v.type = SCP_DOUBLE_VALUE;
        v.u.double_value = expr->u.double_value;
        break;
    case STRING_EXPRESSION:
        v.type = SCP_STRING_VALUE;
        /* 字符数组转scp字符串，引用计数=1 */
        SCP_String *string = alloc_scp_string(expr->u.string_value, SCP_TRUE);
        string->ref_count = 1;
        v.u.string_value = string;      /* 字串赋值给v */
        break;
    case IDENTIFIER_EXPRESSION:
        v = get_identifier_value(inter, env, expr);
        break;
    case ASSIGN_EXPRESSION:
        v = eval_assign_expression(inter, env, expr->u.assign_expression.variable,
                                   expr->u.assign_expression.operand);
        break;
    
    /* 二元表达式计算 */
    case ADD_EXPRESSION:
    case SUB_EXPRESSION:
    case MUL_EXPRESSION:
    case DIV_EXPRESSION:
    case MOD_EXPRESSION:
    case EQ_EXPRESSION:
    case NE_EXPRESSION:
    case GT_EXPRESSION:
    case GE_EXPRESSION:
    case LT_EXPRESSION:
    case LE_EXPRESSION:
        v = scp_eval_binary_expression(inter, env, expr->type, expr->u.binary_expression.left,
                                       expr->u.binary_expression.right);
        break;
    /* 逻辑与或计算 */
    case LOGICAL_AND_EXPRESSION:
    case LOGICAL_OR_EXPRESSION:
        v = eval_logical_and_or_expression(inter, env, expr->type, expr->u.binary_expression.left,
                                           expr->u.binary_expression.right);
        break;
    case MINUS_EXPRESSION:
        v = scp_eval_minus_expression(inter, env, expr->u.minus_expression);
        break;
    case FUNCTION_CALL_EXPRESSION:
        v = eval_function_call_expression(inter, env, expr);
        break;
    case NULL_EXPRESSION:
        v.type = SCP_NULL_VALUE;
        break;
    case EXPRESSION_TYPE_COUNT_PLUS_1:  /* FALLTHROUGH 跌落 */
    default:
        DBG_panic(("bad case. type..%d\n", expr->type));
    }
    return v;
}

/* 计算表达式接口*/
SCP_Value scp_eval_expression(SCP_Interpreter *inter, LocalEnvironment *env, Expression *expr)
{
    return eval_expression(inter, env, expr);
}

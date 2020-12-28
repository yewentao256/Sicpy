#include <math.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "sicpy.h"

static StatementResult execute_statement(SCP_Interpreter *inter,
                                         LocalEnvironment *env, Statement *statement);

/* 执行表达式语句 */
static StatementResult execute_expression_statement(SCP_Interpreter *inter, LocalEnvironment *env,
                             Statement *statement)
{
    StatementResult result;
    result.type = NORMAL_STATEMENT_RESULT;
    /* 计算表达式值 */
    SCP_Value v = scp_eval_expression(inter, env, statement->u.expression_s);
    /* 如果是字符串类型，进行释放 */
    if (v.type == SCP_STRING_VALUE) {
        scp_release_string(v.u.string_value);
    }

    return result;
}

/* 执行global语句 */
static StatementResult execute_global_statement(SCP_Interpreter *inter, LocalEnvironment *env,
                         Statement *statement)
{
    IdentifierList *pos;
    StatementResult result;

    result.type = NORMAL_STATEMENT_RESULT;
    /* 如果没有局部变量环境，报错 */
    if (env == NULL) {
        scp_runtime_error(statement->line_number,
                          GLOBAL_STATEMENT_IN_TOPLEVEL_ERR, MESSAGE_ARGUMENT_END);
    }
    /* 遍历全局变量标识符链表 */
    for (pos = statement->u.global_identifier_list; pos; pos = pos->next) {
        GlobalVariableRef *global_v_ref;
        /* 遍历局部环境的全局变量链表，如果发现已有全局变量标识符，则到NEXT_IDENTIFIER */
        for (global_v_ref = env->global_variable; global_v_ref; global_v_ref = global_v_ref->next) {
            if (!strcmp(global_v_ref->variable->name, pos->name))
                goto NEXT_IDENTIFIER;
        }
        /* 搜索当前标识符的变量 */
        Variable *variable = scp_search_global_variable(inter, pos->name);
        if (variable == NULL) {
            scp_runtime_error(statement->line_number, GLOBAL_VARIABLE_NOT_FOUND_ERR,
                              STRING_MESSAGE_ARGUMENT, "name", pos->name, MESSAGE_ARGUMENT_END);
        }
        /* 创建新引用，将该变量头插加入局部环境全局变量链表 */
        GlobalVariableRef *new_ref = MEM_malloc(sizeof(GlobalVariableRef));
        new_ref->variable = variable;
        new_ref->next = env->global_variable;
        env->global_variable = new_ref;
      NEXT_IDENTIFIER:;
    }

    return result;
}

/* 执行elif语句 */
static StatementResult execute_elif(SCP_Interpreter *inter, LocalEnvironment *env,
              Elif *elif_list, SCP_Boolean *executed)
{
    StatementResult result;
    SCP_Value   cond;
    Elif *pos;
    *executed = SCP_FALSE;
    result.type = NORMAL_STATEMENT_RESULT;
    /* 对于elif链表的每个元素，进行elif运算 */
    for (pos = elif_list; pos; pos = pos->next) {
        cond = scp_eval_expression(inter, env, pos->condition);
        if (cond.type != SCP_BOOLEAN_VALUE) {
            scp_runtime_error(pos->condition->line_number,
                              NOT_BOOLEAN_TYPE_ERR, MESSAGE_ARGUMENT_END);
        }
        if (cond.u.boolean_value) {
            result = scp_execute_statement_list(inter, env, pos->block->statement_list);
            *executed = SCP_TRUE;
            if (result.type != NORMAL_STATEMENT_RESULT)
                break;
        }
    }
    return result;
}

/* 执行if语句 */
static StatementResult execute_if_statement(SCP_Interpreter *inter, LocalEnvironment *env,
                     Statement *statement)
{
    StatementResult result;
    SCP_Value   cond = scp_eval_expression(inter, env, statement->u.if_block.condition);
    result.type = NORMAL_STATEMENT_RESULT;
    /* 条件计算后不是布尔值报错 */
    if (cond.type != SCP_BOOLEAN_VALUE) {
        scp_runtime_error(statement->u.if_block.condition->line_number,
                          NOT_BOOLEAN_TYPE_ERR, MESSAGE_ARGUMENT_END);
    }
    DBG_assert(cond.type == SCP_BOOLEAN_VALUE, ("cond.type..%d", cond.type));

    /* 条件值为真，执行if语句链表 */
    if (cond.u.boolean_value) {
        result = scp_execute_statement_list(inter, env,
                                            statement->u.if_block.then_block ->statement_list);
    }
    else {
        SCP_Boolean elif_executed;
        result = execute_elif(inter, env, statement->u.if_block.elif_list, &elif_executed);
        if (result.type != NORMAL_STATEMENT_RESULT)
            goto FUNC_END;
        /* elif语句没有执行且if语句块存在else块，执行else块 */
        if (!elif_executed && statement->u.if_block.else_block) {
            result = scp_execute_statement_list(inter, env,
                                                statement->u.if_block.else_block ->statement_list);
        }
    }

  FUNC_END:
    return result;
}

/* 执行while语句 */
static StatementResult execute_while_statement(SCP_Interpreter *inter, LocalEnvironment *env,
                        Statement *statement)
{
    StatementResult result;
    SCP_Value   cond;

    result.type = NORMAL_STATEMENT_RESULT;
    for (;;) {
        cond = scp_eval_expression(inter, env, statement->u.while_block.condition);
        if (cond.type != SCP_BOOLEAN_VALUE) {
            scp_runtime_error(statement->u.while_block.condition->line_number,
                              NOT_BOOLEAN_TYPE_ERR, MESSAGE_ARGUMENT_END);
        }
        DBG_assert(cond.type == SCP_BOOLEAN_VALUE, ("cond.type..%d", cond.type));
        /* 条件非真值退出 */
        if (!cond.u.boolean_value)
            break;

        result = scp_execute_statement_list(inter, env,
                                            statement->u.while_block.block ->statement_list);
        /* 执行return则退出 */
        if (result.type == RETURN_STATEMENT_RESULT) {
            break;
        }
        /* 执行break则退出 */
        else if (result.type == BREAK_STATEMENT_RESULT) {
            result.type = NORMAL_STATEMENT_RESULT;
            break;
        }
    }

    return result;
}

/* 执行for语句 */
static StatementResult execute_for_statement(SCP_Interpreter *inter, LocalEnvironment *env,
                      Statement *statement)
{
    StatementResult result;
    SCP_Value   cond;

    result.type = NORMAL_STATEMENT_RESULT;

    /* 如果init中有值或表达式，进行计算 */
    if (statement->u.for_block.init) {
        scp_eval_expression(inter, env, statement->u.for_block.init);
    }
    for (;;) {
        if (statement->u.for_block.condition) {
            cond = scp_eval_expression(inter, env, statement->u.for_block.condition);
            if (cond.type != SCP_BOOLEAN_VALUE) {
                scp_runtime_error(statement->u.for_block.condition->line_number,
                                  NOT_BOOLEAN_TYPE_ERR, MESSAGE_ARGUMENT_END);
            }
            DBG_assert(cond.type == SCP_BOOLEAN_VALUE, ("cond.type..%d", cond.type));
            if (!cond.u.boolean_value)
                break;
        }
        result = scp_execute_statement_list(inter, env,
                                            statement->u.for_block.block ->statement_list);
        /* 执行return或break则退出 */
        if (result.type == RETURN_STATEMENT_RESULT) {
            break;
        }
        else if (result.type == BREAK_STATEMENT_RESULT) {
            result.type = NORMAL_STATEMENT_RESULT;
            break;
        }

        /* 如果post（for循环括号中第三部分）中有语句，执行 */
        if (statement->u.for_block.post) {
            scp_eval_expression(inter, env, statement->u.for_block.post);
        }
    }

    return result;
}

/* 执行返回语句 */
static StatementResult execute_return_statement(SCP_Interpreter *inter, LocalEnvironment *env,
                         Statement *statement)
{
    StatementResult result;
    result.type = RETURN_STATEMENT_RESULT;
    /* 有表达式则执行，否则返回空 */
    if (statement->u.return_expression) {
        result.return_value = scp_eval_expression(inter, env, statement->u.return_expression);
    }
    else {
        result.return_value.type = SCP_NULL_VALUE;
    }

    return result;
}

/* 执行语句 */
static StatementResult execute_statement(SCP_Interpreter *inter, LocalEnvironment *env,
                  Statement *statement)
{
    StatementResult result;
    result.type = NORMAL_STATEMENT_RESULT;

    /* 根据语句类型执行语句 */
    switch (statement->type) {
    case EXPRESSION_STATEMENT:
        result = execute_expression_statement(inter, env, statement);
        break;
    case GLOBAL_STATEMENT:
        result = execute_global_statement(inter, env, statement);
        break;
    case IF_STATEMENT:
        result = execute_if_statement(inter, env, statement);
        break;
    case WHILE_STATEMENT:
        result = execute_while_statement(inter, env, statement);
        break;
    case FOR_STATEMENT:
        result = execute_for_statement(inter, env, statement);
        break;
    case RETURN_STATEMENT:
        result = execute_return_statement(inter, env, statement);
        break;
    case BREAK_STATEMENT:
        result.type = BREAK_STATEMENT_RESULT;
        break;
    case CONTINUE_STATEMENT:
        result.type = CONTINUE_STATEMENT_RESULT;
        break;
    case STATEMENT_TYPE_COUNT_PLUS_1:   /* FALLTHRU */
    default:
        DBG_panic(("bad case...%d", statement->type));
    }

    return result;
}

/* 执行语句列表 */
StatementResult scp_execute_statement_list(SCP_Interpreter *inter, LocalEnvironment *env,
                           StatementList *list)
{
    StatementList *pos;
    StatementResult result;

    result.type = NORMAL_STATEMENT_RESULT;
    for (pos = list; pos; pos = pos->next) {
        result = execute_statement(inter, env, pos->statement);
        /* 如果不是正常的返回结果，则退出执行列表 */
        if (result.type != NORMAL_STATEMENT_RESULT)
            break;
    }
    return result;
}

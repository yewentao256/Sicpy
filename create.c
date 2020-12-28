#include "MEM.h"
#include "DBG.h"
#include "sicpy.h"

/* 定义函数 */
void scp_define_function(char *identifier, ParameterList *parameter_list, Block *block)
{
    /* 如果已有该函数定义，则报错 */
    if (scp_search_function(identifier)) {
        scp_compile_error(FUNCTION_MULTIPLE_DEFINE_ERR,
                          STRING_MESSAGE_ARGUMENT, "name", identifier, MESSAGE_ARGUMENT_END);
        return;
    }
    SCP_Interpreter *inter = scp_get_interpreter();
    FunctionDefinition *f = scp_malloc(sizeof(FunctionDefinition));
    f->name = identifier;
    f->type = SICPY_FUNCTION_DEFINITION;
    f->u.sicpy_f.parameter = parameter_list;
    f->u.sicpy_f.block = block;
    /* 头插法将函数加入函数链表 */
    f->next = inter->function_list;
    inter->function_list = f;
}

/* 传入标识符，创建单个参数链表 */
ParameterList * scp_create_one_parameter_list(char *identifier)
{
    ParameterList *p =scp_malloc(sizeof(ParameterList));
    p->name = identifier;
    p->next = NULL;
    return p;
}

/* 传入参数链表和标识符，连接并返回新链表 */
ParameterList * scp_chain_parameter_list(ParameterList *list, char *identifier)
{
    ParameterList *pos;
    for (pos = list; pos->next; pos = pos->next);
    pos->next = scp_create_one_parameter_list(identifier);

    return list;
}

/* 创建单个实参链表 */
ArgumentList * scp_create_one_argument_list(Expression *expression)
{
    ArgumentList *al= scp_malloc(sizeof(ArgumentList));
    al->expression = expression;
    al->next = NULL;
    return al;
}

/* 创建单个实参链表并连接现有实参链表 */
ArgumentList * scp_chain_argument_list(ArgumentList *list, Expression *expr)
{
    ArgumentList *pos;
    for (pos = list; pos->next; pos = pos->next);
    pos->next = scp_create_one_argument_list(expr);
    return list;
}

/* 传入语句，创建单语句链表 */
StatementList * scp_create_one_statement_list(Statement *statement)
{
    StatementList *sl = scp_malloc(sizeof(StatementList));
    sl->statement = statement;
    sl->next = NULL;
    return sl;
}

/* 连接语句链表 */
StatementList * scp_chain_statement_list(StatementList *list, Statement *statement)
{
    StatementList *pos;

    /* 当前语句链表为空则创建新链表 */
    if (list == NULL)
        return scp_create_one_statement_list(statement);

    /* pos遍历至链表尾 */
    for (pos = list; pos->next; pos = pos->next);
    pos->next = scp_create_one_statement_list(statement);

    return list;
}

/* 传入表达式类型，创建表达式 */
Expression * scp_alloc_expression(ExpressionType type)
{
    Expression  *exp = scp_malloc(sizeof(Expression));
    exp->type = type;
    exp->line_number = scp_get_interpreter()->current_line_number;

    return exp;
}

/* 创建赋值表达式，传入变量（identifier）和操作数（等号右边表达式），返回新表达式 */
Expression * scp_create_assign_expression(char *variable, Expression *operand)
{
    Expression *exp = scp_alloc_expression(ASSIGN_EXPRESSION);
    /* 对应变量和操作数赋值 */
    exp->u.assign_expression.variable = variable;
    exp->u.assign_expression.operand = operand;

    return exp;
}

/* 给表达式赋值 */
static Expression assign_value_to_expression(SCP_Value *v)
{
    Expression  expr;
    /* 如果是int值 */
    if (v->type == SCP_INT_VALUE) {
        expr.type = INT_EXPRESSION;
        expr.u.int_value = v->u.int_value;
    }
    /* 如果是double值 */
    else if (v->type == SCP_DOUBLE_VALUE) {
        expr.type = DOUBLE_EXPRESSION;
        expr.u.double_value = v->u.double_value;
    }
    /* 如果是double值 */
    else {
        DBG_assert(v->type == SCP_BOOLEAN_VALUE,("v->type..%d\n", v->type));
        expr.type = BOOLEAN_EXPRESSION;
        expr.u.boolean_value = v->u.boolean_value;
    }
    return expr;
}

/* 创建二元表达式 */
Expression * scp_create_binary_expression(ExpressionType operator, Expression *left, Expression *right)
{   
    /* 如果是数值类型 */
    if ((left->type == INT_EXPRESSION || left->type == DOUBLE_EXPRESSION)
        && (right->type == INT_EXPRESSION || right->type == DOUBLE_EXPRESSION)) {
        SCP_Value v = scp_eval_binary_expression(scp_get_interpreter(), NULL, operator, left, right);
        /* 将值赋给左式. */
        *left = assign_value_to_expression(&v);
        return left;
    }
    else {
        Expression *exp = scp_alloc_expression(operator);
        exp->u.binary_expression.left = left;
        exp->u.binary_expression.right = right;
        return exp;
    }
}

/* 创建一元表达式 */
Expression * scp_create_minus_expression(Expression *exp)
{   
    /* 如果传入表达式为为int或double */
    if (exp->type == INT_EXPRESSION || exp->type == DOUBLE_EXPRESSION) {
        SCP_Value v = scp_eval_minus_expression(scp_get_interpreter(), NULL, exp);
        *exp = assign_value_to_expression(&v);          /* 注意，这里会覆盖原来的exp */
        return exp;
    }
    else {
        Expression *new_exp = scp_alloc_expression(MINUS_EXPRESSION);
        new_exp->u.minus_expression = exp;
        return new_exp;
    }
}

/* 创建函数调用表达式 */
Expression * scp_create_function_call_expression(char *func_name, ArgumentList *argument)
{
    Expression  *exp = scp_alloc_expression(FUNCTION_CALL_EXPRESSION);
    exp->u.function_call_expression.identifier = func_name;
    exp->u.function_call_expression.argument = argument;
    return exp;
}

/* 创建语句 */
Statement * alloc_statement(StatementType type)
{
    Statement *st = scp_malloc(sizeof(Statement));
    st->type = type;
    st->line_number = scp_get_interpreter()->current_line_number;
    return st;
}

/* 创建单个标识符链表 */
IdentifierList * scp_create_global_identifier(char *identifier)
{
    IdentifierList      *i_list = scp_malloc(sizeof(IdentifierList));
    i_list->name = identifier;
    i_list->next = NULL;
    return i_list;
}

/* 连接标识符链表 */
IdentifierList * scp_chain_identifier(IdentifierList *list, char *identifier)
{
    IdentifierList *pos;
    for (pos = list; pos->next; pos = pos->next);
    pos->next = scp_create_global_identifier(identifier);

    return list;
}

/* 创建if语句 */
Statement * scp_create_if_statement(Expression *condition,
                        Block *then_block, Elif *elif_list, Block *else_block)
{
    Statement *st = alloc_statement(IF_STATEMENT);
    st->u.if_block.condition = condition;
    st->u.if_block.then_block = then_block;
    st->u.if_block.elif_list = elif_list;
    st->u.if_block.else_block = else_block;

    return st;
}


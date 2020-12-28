#ifndef PUBLIC_SICPY_H_INCLUDED
#define PRIVATE_SICPY_H_INCLUDED
#include <stdio.h>
#include "MEM.h"
#include "SCP.h"

#define MESSAGE_ARGUMENT_MAX    (256)
#define LINE_BUF_SIZE           (1024)

/* 编译错误类型，注意第一个赋值为0，之后会递增 */
typedef enum {
    PARSE_ERR = 0,
    CHARACTER_INVALID_ERR,
    FUNCTION_MULTIPLE_DEFINE_ERR,
    COMPILE_ERROR_COUNT_PLUS_1
} CompileError;

/* 运行错误类型 */
typedef enum {
    VARIABLE_NOT_FOUND_ERR = 0,
    FUNCTION_NOT_FOUND_ERR,
    ARGUMENT_TOO_MANY_ERR,
    ARGUMENT_TOO_FEW_ERR,
    NOT_BOOLEAN_TYPE_ERR,
    MINUS_OPERAND_TYPE_ERR,
    BAD_OPERAND_TYPE_ERR,
    NOT_BOOLEAN_OPERATOR_ERR,
    FOPEN_ARGUMENT_TYPE_ERR,
    FCLOSE_ARGUMENT_TYPE_ERR,
    FREAD_ARGUMENT_TYPE_ERR,
    FWRITE_ARGUMENT_TYPE_ERR,
    NOT_NULL_OPERATOR_ERR,
    DIVISION_BY_ZERO_ERR,
    GLOBAL_VARIABLE_NOT_FOUND_ERR,
    GLOBAL_STATEMENT_IN_TOPLEVEL_ERR,
    BAD_OPERATOR_FOR_STRING_ERR,
    RUNTIME_ERROR_COUNT_PLUS_1
} RuntimeError;

/* 报错信息类型 */
typedef enum {
    INT_MESSAGE_ARGUMENT = 1,
    DOUBLE_MESSAGE_ARGUMENT,
    STRING_MESSAGE_ARGUMENT,
    CHARACTER_MESSAGE_ARGUMENT,
    POINTER_MESSAGE_ARGUMENT,
    MESSAGE_ARGUMENT_END
} MessageArgumentType;

/* 报错信息参数 */
typedef struct {
    MessageArgumentType type;
    char        *name;
    union {
        int     int_val;
        double  double_val;
        char    *string_val;
        void    *pointer_val;
        int     character_val;
    } u;
} MessageArgument;

typedef enum {
    BOOLEAN_EXPRESSION = 1,
    INT_EXPRESSION,
    DOUBLE_EXPRESSION,
    STRING_EXPRESSION,
    IDENTIFIER_EXPRESSION,
    ASSIGN_EXPRESSION,
    ADD_EXPRESSION,
    SUB_EXPRESSION,
    MUL_EXPRESSION,
    DIV_EXPRESSION,
    MOD_EXPRESSION,
    EQ_EXPRESSION,
    NE_EXPRESSION,
    GT_EXPRESSION,
    GE_EXPRESSION,
    LT_EXPRESSION,
    LE_EXPRESSION,
    LOGICAL_AND_EXPRESSION,
    LOGICAL_OR_EXPRESSION,
    MINUS_EXPRESSION,
    FUNCTION_CALL_EXPRESSION,
    NULL_EXPRESSION,
    EXPRESSION_TYPE_COUNT_PLUS_1
} ExpressionType;

/* 宏定义数学操作符如加号 */
#define dkc_is_math_operator(operator)\
    ((operator) == ADD_EXPRESSION || (operator) == SUB_EXPRESSION\
   || (operator) == MUL_EXPRESSION || (operator) == DIV_EXPRESSION || (operator) == MOD_EXPRESSION)

/* 宏定义逻辑操作符如与 */
#define dkc_is_compare_operator(operator) \
    ((operator) == EQ_EXPRESSION || (operator) == NE_EXPRESSION || (operator) == GT_EXPRESSION\
    || (operator) == GE_EXPRESSION || (operator) == LT_EXPRESSION || (operator) == LE_EXPRESSION)

#define dkc_is_logical_operator(operator) \
  ((operator) == LOGICAL_AND_EXPRESSION || (operator) == LOGICAL_OR_EXPRESSION)

typedef struct Statement_tag Statement;
typedef struct Expression_tag Expression;

typedef struct StatementList_tag {
    Statement   *statement;
    struct StatementList_tag    *next;
} StatementList;

/* 实参列表，内容为表达式，这样可以在实参中传递表达式 */
typedef struct ArgumentList_tag {
    Expression *expression;
    struct ArgumentList_tag *next;
} ArgumentList;

/* 赋值表达式，包括变量和操作符 */
typedef struct {
    char        *variable;
    Expression  *operand;
} AssignExpression;

/* 二值表达式 */
typedef struct {
    Expression  *left;
    Expression  *right;
} BinaryExpression;

/* 函数调用表达式 */
typedef struct {
    char                *identifier;
    ArgumentList        *argument;
} FunctionCallExpression;

/* SCP布尔值 */
typedef enum {
    SCP_FALSE = 0,
    SCP_TRUE = 1
} SCP_Boolean;



/* SCP Value类型 */
typedef enum {
    SCP_BOOLEAN_VALUE = 1,
    SCP_INT_VALUE,
    SCP_DOUBLE_VALUE,
    SCP_STRING_VALUE,
    SCP_NATIVE_POINTER_VALUE,
    SCP_NULL_VALUE
} SCP_ValueType;

/* SCP原生指针 */
typedef struct {
    char  *info;
    void  *pointer;
} SCP_NativePointer;


/* SCP的字符串类型 */
typedef struct SCP_String_tag {
    int         ref_count;      /* 引用计数 */
    char        *string;        /* 字符数组 */
    SCP_Boolean is_literal;     /* 是否需要进行引用计数 */
}SCP_String;

/* SCP基础值类型，包括布尔、int、double、string和指针 */
typedef struct {
    SCP_ValueType       type;
    union {
        SCP_Boolean     boolean_value;
        int             int_value;
        double          double_value;
        SCP_String      *string_value;
        SCP_NativePointer       native_pointer;
    } u;
} SCP_Value;

/* 表达式结构体 */
struct Expression_tag {
    ExpressionType type;
    int line_number;
    union {
        SCP_Boolean             boolean_value;              /* 布尔值 */
        int                     int_value;                  /* int值 */
        double                  double_value;               /* double值 */
        char                    *string_value;              /* string值 */
        char                    *identifier;                /* 标识符 */
        AssignExpression        assign_expression;          /* 赋值表达式 */
        BinaryExpression        binary_expression;          /* 二值表达式 */
        Expression              *minus_expression;          /* 负值表达式 */
        FunctionCallExpression  function_call_expression;   /* 函数调用表达式 */
    } u;
};

typedef struct {
    StatementList       *statement_list;
} Block;

/* 标识符链表 */
typedef struct IdentifierList_tag {
    char        *name;
    struct IdentifierList_tag   *next;
} IdentifierList;

/* ELIF结构体，有表达式、块和下一个elif嵌套 */
typedef struct Elif_tag {
    Expression  *condition;
    Block       *block;
    struct Elif_tag    *next;
} Elif;

/* if语句块，有条件、then块、elif块和else块 */
typedef struct {
    Expression  *condition;
    Block       *then_block;
    Elif        *elif_list;
    Block       *else_block;
} IfBlock;

/* while语句块，有条件和语句块 */
typedef struct {
    Expression  *condition;
    Block       *block;
} WhileBlock;

/* For语句块，有初始化、条件内容、循环中执行内容与语句块 */
typedef struct {
    Expression  *init;
    Expression  *condition;
    Expression  *post;
    Block       *block;
} ForBlock;


/* 语句类型 */
typedef enum {
    EXPRESSION_STATEMENT = 1,
    GLOBAL_STATEMENT,
    IF_STATEMENT,
    WHILE_STATEMENT,
    FOR_STATEMENT,
    RETURN_STATEMENT,
    BREAK_STATEMENT,
    CONTINUE_STATEMENT,
    STATEMENT_TYPE_COUNT_PLUS_1
} StatementType;

/* 语句结构体，包含语句类型、行数等信息 */
struct Statement_tag {
    StatementType       type;
    int                 line_number;
    union {
        Expression      *expression_s;                  /* 表达式 */
        IdentifierList  *global_identifier_list;        /* 全局变量链表 */ 
        IfBlock         if_block;                       /* if代码块 */
        WhileBlock      while_block;                    /* while代码块 */
        ForBlock        for_block;                      /* for代码块 */
        Expression      *return_expression;             /* 返回表达式 */
    } u;
};


/* 形参列表结构体 */
typedef struct ParameterList_tag {
    char        *name;
    struct ParameterList_tag *next;
} ParameterList;

/* 函数定义类型，包含普通函数定义，和预留的被C调用的函数接口 */
typedef enum {
    SICPY_FUNCTION_DEFINITION = 1,
    NATIVE_FUNCTION_DEFINITION
} FunctionDefinitionType;

typedef SCP_Value SCP_NativeFunctionProc(SCP_Interpreter *interpreter, int arg_count, SCP_Value *args);

/* 函数定义结构体 */
typedef struct FunctionDefinition_tag {
    char                *name;
    FunctionDefinitionType      type;
    union {
        struct {
            ParameterList       *parameter;
            Block               *block;
        } sicpy_f;       /* 原生scp函数 */
        struct {
            SCP_NativeFunctionProc      *proc;
        } native_f;         /* C语言函数 */
    } u;
    struct FunctionDefinition_tag       *next;
} FunctionDefinition;

typedef struct Variable_tag {
    char        *name;
    SCP_Value   value;
    struct Variable_tag *next;
} Variable;

typedef enum {
    NORMAL_STATEMENT_RESULT = 1,
    RETURN_STATEMENT_RESULT,
    BREAK_STATEMENT_RESULT,
    CONTINUE_STATEMENT_RESULT,
    STATEMENT_RESULT_TYPE_COUNT_PLUS_1
} StatementResultType;

/* 语句结果结构体 */
typedef struct {
    StatementResultType type;
    SCP_Value   return_value;
} StatementResult;

/* 全局变量链表 */
typedef struct GlobalVariableRef_tag {
    Variable    *variable;
    struct GlobalVariableRef_tag *next;
} GlobalVariableRef;

/* 局部环境，包含普通变量链表和全局变量链表 */
typedef struct {
    Variable    *variable;
    GlobalVariableRef   *global_variable;
} LocalEnvironment;


/* SCP解释器 */
struct SCP_Interpreter_tag {
    MEM_Storage         interpreter_storage;    /* 解释器内存 */
    MEM_Storage         execute_storage;        /* 执行内存 */
    Variable            *variable;              /* 变量 */
    FunctionDefinition  *function_list;         /* 函数定义链表 */
    StatementList       *statement_list;        /* 语句链表 */
    int                 current_line_number;    /* 行号 */
};



void SCP_add_native_function(SCP_Interpreter *interpreter, char *name, SCP_NativeFunctionProc *proc);
void scp_add_global_variable(SCP_Interpreter *inter, char *identifier, SCP_Value *value);

/* create.c */
void scp_define_function(char *identifier, ParameterList *parameter_list, Block *block);
ParameterList *scp_create_one_parameter_list(char *identifier);
ParameterList *scp_chain_parameter_list(ParameterList *list, char *identifier);
ArgumentList *scp_create_one_argument_list(Expression *expression);
ArgumentList *scp_chain_argument_list(ArgumentList *list, Expression *expr);
StatementList *scp_create_one_statement_list(Statement *statement);
StatementList *scp_chain_statement_list(StatementList *list, Statement *statement);
Expression *scp_alloc_expression(ExpressionType type);
Expression *scp_create_assign_expression(char *variable, Expression *operand);
Expression *scp_create_binary_expression(ExpressionType operator,Expression *left, Expression *right);
Expression *scp_create_minus_expression(Expression *operand);
Expression *scp_create_function_call_expression(char *func_name, ArgumentList *argument);
IdentifierList *scp_create_global_identifier(char *identifier);
IdentifierList *scp_chain_identifier(IdentifierList *list, char *identifier);
Statement *scp_create_if_statement(Expression *condition,
                                    Block *then_block, Elif *elif_list,Block *else_block);

/* string.c */
void scp_add_character(int letter);
void scp_reset_string_buffer(void);
char *scp_close_string(void);

/* execute.c */
StatementResult scp_execute_statement_list(SCP_Interpreter *inter,
                           LocalEnvironment *env, StatementList *list);

/* eval.c */
SCP_Value scp_eval_binary_expression(SCP_Interpreter *inter, LocalEnvironment *env,
                                 ExpressionType operator, Expression *left, Expression *right);
SCP_Value scp_eval_minus_expression(SCP_Interpreter *inter,
                                LocalEnvironment *env, Expression *operand);
SCP_Value scp_eval_expression(SCP_Interpreter *inter, LocalEnvironment *env, Expression *expr);

/* string_pool.c */
void scp_release_string(SCP_String *str);
SCP_String *scp_create_sicpy_string(char *str);
SCP_String * alloc_scp_string(char *str, SCP_Boolean is_literal);

/* util.c */
SCP_Interpreter *scp_get_interpreter(void);
void scp_set_current_interpreter(SCP_Interpreter *inter);
void *scp_malloc(size_t size);
Variable *scp_search_local_variable(LocalEnvironment *env, char *identifier);
Variable * scp_search_global_variable(SCP_Interpreter *inter, char *identifier);
void scp_add_local_variable(LocalEnvironment *env, char *identifier, SCP_Value *value);
SCP_NativeFunctionProc * scp_search_native_function(SCP_Interpreter *inter, char *name);
FunctionDefinition *scp_search_function(char *name);
char *scp_get_operator_string(ExpressionType type);

/* error.c */
void scp_compile_error(CompileError id, ...);
void scp_runtime_error(int line_number, RuntimeError id, ...);

/* native.c */
SCP_Value scp_nv_print_proc(SCP_Interpreter *interpreter, int arg_count, SCP_Value *args);
SCP_Value scp_nv_fopen_proc(SCP_Interpreter *interpreter, int arg_count, SCP_Value *args);
SCP_Value scp_nv_fclose_proc(SCP_Interpreter *interpreter, int arg_count, SCP_Value *args);
SCP_Value scp_nv_fread_proc(SCP_Interpreter *interpreter, int arg_count, SCP_Value *args);
SCP_Value scp_nv_fwrite_proc(SCP_Interpreter *interpreter, int arg_count, SCP_Value *args);
void scp_add_std_fp(SCP_Interpreter *inter);

#endif /* PRIVATE_SICPY_H_INCLUDED */

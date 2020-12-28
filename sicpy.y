%{
#include <stdio.h>
#include "sicpy.h"
#define YYDEBUG 1
%}

%union {
    char                *identifier;        /* 标识符 */
    ParameterList       *parameter_list;    /* 形参链表 */
    ArgumentList        *argument_list;     /* 实参链表 */
    Expression          *expression;        /* 表达式 */
    Statement           *statement;         /* 语句 */
    StatementList       *statement_list;    /* 语句链表 */
    Block               *block;             /* 块，包含语句链表 */
    Elif                *elif;              /* elif表达式 */
    IdentifierList      *identifier_list;   /* 标识符链表 */
}

%token <expression>     INT_TOKEN DOUBLE_TOKEN STRING_TOKEN
%token <identifier>     IDENTIFIER
%token FUNCTION IF ELSE ELIF WHILE FOR RETURN_T BREAK CONTINUE NULL_T
        LP RP LC RC SEMICOLON COMMA ASSIGN LOGICAL_AND LOGICAL_OR
        EQ NE GT GE LT LE ADD SUB MUL DIV MOD TRUE_T FALSE_T GLOBAL_T
%type   <parameter_list> parameter_list
%type   <argument_list> argument_list
%type   <expression> expression expression_opt
        logical_and_expression logical_or_expression equality_expression relational_expression
        additive_expression multiplicative_expression unary_expression primary_expression
%type   <statement> statement global_statement if_statement while_statement
        for_statement return_statement break_statement continue_statement
%type   <statement_list> statement_list
%type   <block> block
%type   <elif> elif elif_list
%type   <identifier_list> identifier_list

%%
/* 最顶层单元 */
translation_unit: definition_or_statement
        | translation_unit definition_or_statement
        ;

/* 定义或语句 */
definition_or_statement: function_definition
        | statement {
            SCP_Interpreter *inter = scp_get_interpreter();
            /* 传入语句，链接现有语句链表 */
            inter->statement_list = scp_chain_statement_list(inter->statement_list, $1);
        };

/* 函数定义 */
function_definition: FUNCTION IDENTIFIER LP parameter_list RP block {
            /* 形如function func(a = 0){} */
            /* 传入标识符、参数链表和语句块 */
            scp_define_function($2, $4, $6);
        }
        | FUNCTION IDENTIFIER LP RP block        {
            /* 形如function func(){} */
            /* 传入标识符、空（参数链表）和语句块 */
            scp_define_function($2, NULL, $5);
        };

/* 参数链表 */
parameter_list: IDENTIFIER{
            /* 传入标识符，创建长为1的参数链表 */
            $$ = scp_create_one_parameter_list($1);
        }
        | parameter_list COMMA IDENTIFIER {
            /* 参数列表形如a=1,b=2 */
            /* 传入现有参数链表和新标识符，进行连接 */
            $$ = scp_chain_parameter_list($1, $3);
        };

/* 实参链表 */
argument_list: expression {
            /* 传入标识符，创建长为1的参数链表 */
            $$ = scp_create_one_argument_list($1);
        }
        | argument_list COMMA expression {
            /* 传入现有参数链表和新标识符，进行连接 */
            $$ = scp_chain_argument_list($1, $3);
        };

/* 语句链表 */
statement_list: statement {
            /* 传入标识符，创建长为1的语句链表 */
            $$ = scp_create_one_statement_list($1);
        }
        | statement_list statement
        {
            /* 传入现有语句链表和新标识符，进行连接 */
            $$ = scp_chain_statement_list($1, $2);
        };

/* 表达式 */
expression: logical_or_expression
        | IDENTIFIER ASSIGN expression {
            /* 形如a=3或a=b+3 */
            /* 传入标识符和表达式，创建新表达式 */
            $$ = scp_create_assign_expression($1, $3);
        };

/* 逻辑或表达式 */
logical_or_expression: logical_and_expression
        | logical_or_expression LOGICAL_OR logical_and_expression {   
            /* 传入“或”表达式类型与||两边的表达式 */
            $$ = scp_create_binary_expression(LOGICAL_OR_EXPRESSION, $1, $3);
        };

/* 逻辑与表达式 */
logical_and_expression: equality_expression
        | logical_and_expression LOGICAL_AND equality_expression {
            /* 传入“与”表达式类型与&&两边的表达式 */
            $$ = scp_create_binary_expression(LOGICAL_AND_EXPRESSION, $1, $3);
        };

/* 逻辑相等表达式 */
equality_expression: relational_expression
        | equality_expression EQ relational_expression {
            /* 传入“逻辑相等”表达式类型与==两边的表达式 */
            $$ = scp_create_binary_expression(EQ_EXPRESSION, $1, $3);
        }
        | equality_expression NE relational_expression {
            /* 传入“逻辑不等”表达式类型与!=两边的表达式 */
            $$ = scp_create_binary_expression(NE_EXPRESSION, $1, $3);
        };

/* 逻辑大小表达式 */
relational_expression: additive_expression
        | relational_expression GT additive_expression {
            /* 大于 */
            $$ = scp_create_binary_expression(GT_EXPRESSION, $1, $3);
        }
        | relational_expression GE additive_expression {
            /* 大于等于 */
            $$ = scp_create_binary_expression(GE_EXPRESSION, $1, $3);
        }
        | relational_expression LT additive_expression {
            /* 小于 */
            $$ = scp_create_binary_expression(LT_EXPRESSION, $1, $3);
        }
        | relational_expression LE additive_expression {
            /* 小于等于 */
            $$ = scp_create_binary_expression(LE_EXPRESSION, $1, $3);
        };

/* 加性表达式 */
additive_expression: multiplicative_expression
        | additive_expression ADD multiplicative_expression {
            /* 加法 */
            $$ = scp_create_binary_expression(ADD_EXPRESSION, $1, $3);
        }
        | additive_expression SUB multiplicative_expression {
            /* 减法 */
            $$ = scp_create_binary_expression(SUB_EXPRESSION, $1, $3);
        };

/* 乘除表达式 */
multiplicative_expression: unary_expression
        | multiplicative_expression MUL unary_expression {
            /* 乘法 */
            $$ = scp_create_binary_expression(MUL_EXPRESSION, $1, $3);
        }
        | multiplicative_expression DIV unary_expression {
            /* 除法 */
            $$ = scp_create_binary_expression(DIV_EXPRESSION, $1, $3);
        }
        | multiplicative_expression MOD unary_expression {
            /* 取余 */
            $$ = scp_create_binary_expression(MOD_EXPRESSION, $1, $3);
        };

/* 一元（取负）表达式 */
unary_expression: primary_expression
        | SUB unary_expression {
            $$ = scp_create_minus_expression($2);
        };

/* 原子表达式 */
primary_expression: IDENTIFIER LP argument_list RP {   
            /* 调用函数，形如func(a=1) */
            $$ = scp_create_function_call_expression($1, $3);
        }
        | IDENTIFIER LP RP {   
            /* 调用无参函数，形如func() */
            $$ = scp_create_function_call_expression($1, NULL);
        }
        | LP expression RP {
            /* 对括号的处理 */
            $$ = $2;
        }
        | IDENTIFIER {
            /* 形如单个标识符 */
            Expression *exp = scp_alloc_expression(IDENTIFIER_EXPRESSION);
            exp->u.identifier = $1;
            $$ = exp;
        }
        | INT_TOKEN
        | DOUBLE_TOKEN
        | STRING_TOKEN
        | TRUE_T {
            /* 创建布尔表达式 */
            Expression *exp = scp_alloc_expression(BOOLEAN_EXPRESSION);
            exp->u.boolean_value = SCP_TRUE;
            $$ = exp;
        }
        | FALSE_T {
            /* 创建布尔表达式 */
            Expression *exp = scp_alloc_expression(BOOLEAN_EXPRESSION);
            exp->u.boolean_value = SCP_FALSE;
            $$ = exp;
        }
        | NULL_T {
            /* 创建空表达式 */
            $$ = scp_alloc_expression(NULL_EXPRESSION);
        };

/* 语句 */
statement: expression SEMICOLON {   
            /* 形如表达式加分号 */
            Statement *st = alloc_statement(EXPRESSION_STATEMENT);
            st->u.expression_s = $1;
            $$ = st;
        }
        | global_statement
        | if_statement
        | while_statement
        | for_statement
        | return_statement
        | break_statement
        | continue_statement
        ;

/* 声明全局变量语句 */
global_statement: GLOBAL_T identifier_list SEMICOLON {
            /* 形如global a; */
            Statement *st = alloc_statement(GLOBAL_STATEMENT);
            st->u.global_identifier_list = $2;
            $$ = st;
        };

/* 标识符链表 */
identifier_list: IDENTIFIER {
            /* 单个标识符 */
            $$ = scp_create_global_identifier($1);
        }
        | identifier_list COMMA IDENTIFIER {
            /* 多个标识符 a,b */
            $$ = scp_chain_identifier($1, $3);
        };

/* if语句 */
if_statement: IF LP expression RP block {
            /* 形如if(3<5){} */
            $$ = scp_create_if_statement($3, $5, NULL, NULL);
        }
        | IF LP expression RP block ELSE block {
            /* 形如if(2>4){} else{} */
            $$ = scp_create_if_statement($3, $5, NULL, $7);
        }
        | IF LP expression RP block elif_list {
            /* 形如if(2>4){} elif{} elif{} */
            $$ = scp_create_if_statement($3, $5, $6, NULL);
        }
        | IF LP expression RP block elif_list ELSE block {
            /* 形如if(2>4){} elif{} elif{} else{} */
            $$ = scp_create_if_statement($3, $5, $6, $8);
        };

/* elif链表 */
elif_list: elif
        | elif_list elif{
            /* 尾插法串联多个elif成链表 */
            Elif *pos;
            for (pos = $1; pos->next; pos = pos->next);
            pos->next = $2;
            $$ = $1;
        };

/* 单个elif */
elif: ELIF LP expression RP block{
            /* 创建单个elif结构 */
            Elif *eiif = scp_malloc(sizeof(Elif));
            eiif->condition = $3;
            eiif->block = $5;
            eiif->next = NULL;
            $$ = eiif;
        };

/* while语句 */
while_statement: WHILE LP expression RP block{
            /* 形如while(a>5){} */
            Statement *st = alloc_statement(WHILE_STATEMENT);
            st->u.while_block.condition = $3;
            st->u.while_block.block = $5;
            $$ = st;
        };

/* for循环语句 */
for_statement: FOR LP expression_opt SEMICOLON expression_opt SEMICOLON expression_opt RP block{
            /* 形如for(a=1; a>2; a=a+1){} */
            Statement *st = alloc_statement(FOR_STATEMENT);
            st->u.for_block.init = $3;
            st->u.for_block.condition = $5;
            st->u.for_block.post = $7;
            st->u.for_block.block = $9;
            $$ = st;
        };

/* for循环中的部分表达式（可以为空） */
expression_opt: /* 空 */
        {
            $$ = NULL;
        }
        | expression;

/* 返回语句 */
return_statement: RETURN_T expression_opt SEMICOLON{
            /* 形如return a; */
            Statement *st = alloc_statement(RETURN_STATEMENT);
            st->u.return_expression = $2;
            $$ = st;
        };

/* break语句 */
break_statement: BREAK SEMICOLON{
            /* 形如break; */
            $$ = alloc_statement(BREAK_STATEMENT);
        };

/* continue语句 */
continue_statement: CONTINUE SEMICOLON{
            /* 形如continue; */
            $$ = alloc_statement(CONTINUE_STATEMENT);
        };

/* 代码块 */
block: LC statement_list RC{
            /* 形如{a=2; b=3;} */
            Block *block = scp_malloc(sizeof(Block));
            block->statement_list = $2;
            $$ = block;
        }
        | LC RC {
            /* 空代码块 */
            Block *block = scp_malloc(sizeof(Block));
            block->statement_list = NULL;
            $$ = block;
        };
%%

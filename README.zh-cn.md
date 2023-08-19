# SCP/sicpy

[English](README.md) | 简体中文

- [SCP/sicpy](#scpsicpy)
  - [简介](#简介)
  - [开发细节](#开发细节)
    - [代码量统计](#代码量统计)
    - [词法分析](#词法分析)
    - [语法分析](#语法分析)
    - [语义分析](#语义分析)
    - [核心数据类型](#核心数据类型)
    - [计算模块](#计算模块)
    - [debug模块](#debug模块)
    - [memory模块](#memory模块)
    - [sicpy原生函数与C语言函数预留接口](#sicpy原生函数与c语言函数预留接口)
  - [使用手册](#使用手册)
    - [编译及运行](#编译及运行)
    - [语言描述](#语言描述)
    - [输入输出样例](#输入输出样例)
  - [参考资料](#参考资料)

## 简介

综述：**SCP**即simple C and python，我命名为**sicpy**，sicpy是基于C语言和python特点所综合的一门**无类型语言**，使用纯C语言开发。
下面是一些语言特点：

- 变量不需要设定类型，编译器会自动猜测变量类型。
- 使用`if`、`elif`、`else`来取代`switch`
- 局部区域访问全局变量必须加上**global**
- 原生string类型通过**引用计数**方式自动垃圾回收，可以通过 **+** 连接
- sicpy原生函数如`print`、`fopen`、`fwrite`、`fread`、`fclose`，给C函数调用预留了接口
- 面向解释器开发人员的DBG模块（assert和panic）与面向程序员的精确到行数的报错模块（runtime和compile报错）
- ……更多特性等待探索

## 开发细节

### 代码量统计

Date : 2020-12-26 10:03:11

Total : 19 files,  2474 codes, 380 comments, 375 blanks, all 3229 lines

| language | files | code | comment | blank | total |
| :--- | ---: | ---: | ---: | ---: | ---: |
| C | 11 | 1,663 | 230 | 231 | 2,124 |
| C++ | 4 | 363 | 44 | 85 | 492 |
| Yacc/Bison | 1 | 240 | 79 | 31 | 350 |
| Lex/Flex | 1 | 151 | 26 | 26 | 203 |
| Makefile | 1 | 45 | 1 | 2 | 48 |
| JSON with Comments | 1 | 12 | 0 | 0 | 12 |

### 词法分析

- 全局变量：用于存放flex处理的字符串流

```c
#define STRING_ALLOC_SIZE       (256)   /* 每次buffer不够，新增的buffersize */
static char *flex_string_buffer = NULL;
static int flex_string_buffer_size = 0;
static int flex_string_buffer_alloc_size = 0;
```

- 关键函数：flex中调用，实现与全局变量间的交互

```c
/* 给字符串添加一个新字符 */
void scp_add_character(int letter)
/* 清空字串缓存 */
void scp_reset_string_buffer(void)
/* 关闭字符串，在字符串末尾加上\0 */
char * scp_close_string(void)
```

- flex状态：包括**INITIAL**，**COMMENT**，**STRING**三类
  - **INITIAL**为通常状态，正常处理标识符、括号数值等，遇到#开启注释状态，遇到`"`开启字符串状态
  - **COMMENT**为注释状态，此时所有字符均丢弃，遇到`\n`则回到INITIAL状态
  - **STRING**为字符串状态，上文所述全局变量和关键函数均在此处使用，再次遇到`"`回到**INITIAL**状态

- 样例说明

```c
<INITIAL># {
    /* #意味着开始注释模式 */
    BEGIN COMMENT;
}
<COMMENT>\n {
    /* 在注释中遇到换行符，说明注释结束，并开始初始状态 */
    increment_line_number();
    BEGIN INITIAL;
}
<INITIAL>\" {
    /* 匹配字符串的开始，缓冲区设为0 */
    flex_string_buffer_size = 0;
    BEGIN STRING;
}
<STRING>\" {
    /* 字符串状态遇到"说明字符串结束，该字符串整体加入表达式 */
    Expression *expression = scp_alloc_expression(STRING_EXPRESSION);
    // scp_close_string()作用为copy当前字符串，且在末尾加上\0
    expression->u.string_value = scp_close_string();
    yylval.expression = expression;
    BEGIN INITIAL;     // 返回通常状态
    return STRING_TOKEN;
}
```

### 语法分析

- 核心数据结构

```c
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
```

- 对于表达式计算采用**递推式文法设计**

注：优先级越低，在文法设计中层级越高，越后面进行计算

```c
expression -> logical_or_expression -> logical_and_expression -> equality_expression
-> relational_expression -> additive_expression -> multiplicative_expression 
-> unary_expression -> primary_expression
```

- `statment_list`——语句链表

  - 语句（statement）解析为表达式+分号，形如：`expression SEMICOLON`，由以下7种语句类型构成

    ```c
        | global_statement
        | if_statement
        | while_statement
        | for_statement
        | return_statement
        | break_statement
        | continue_statement
    ```

  - 语句链表将语句串联，形成整体结构便于调用

- `block`——代码块的应用

  - 代码块有两种，一种包含语句链表形如{语句链表}：`LC statement_list RC`，一种为空代码块形如{}：`LC RC`
  - 代码块声明完成后，可以在函数定义/循环等多处灵活使用

- `argument_list`与`parameter_list`——实参和形参链表
  - 实参或形参链表由参数构成
  - 如果是单一的标识符或表达式传入，形如`identifier`（形参），`expression`（实参），则创建对应链表
  - 如果已有链表，则进行连接

- `function`——函数定义及参数传递，根据有无参数传递，有两种文法模式：
  - 通过`function identifier() {}`
  - 通过`function identifier(parameter_list) {}`

- `if_statement`——if语句，if语句有四种解析模式：
    1. 单if，形如 `IF LP expression RP block`
    2. if+else，形如`IF LP expression RP block ELSE block`
    3. if+elif链表，形如`IF LP expression RP block elif_list`
    4. if+elif链表+else，形如`IF LP expression RP block elif_list ELSE block`

- `elif`——elif表达式实现
  - elif基本与if语句相同，都是将表达式存入对应数据结构
  - 但elif不同之处在于，elif是无限延伸的（一个if可以有无数个elif），因此我们需要使用elif_list串联语句
  - 串联思路与statement链表、参数链表相同此处不过多赘述。

### 语义分析

- 语义分析的基本思想是**将表达式分配内存空间，存储入对应的数据结构**，在合适的时间点再进行运算或处理。
- 为什么不直接进行计算呢？因为我们是解释型语言，需要根据上下文**推测类型**，且直接计算会导致**假短路**
  - 什么是假短路呢？例如，我们直接在分析`additive_expression`时进行加法计算，这样因为逻辑计算次序问题，最后传到`logical_and_expression LOGICAL_AND equality_expression`时，我们的左右表达式**已经计算完毕**，这时候再进行逻辑短路操作，实际是没有意义的。

- 例子：对block的语义处理，我们分配内存，存储对应的语句链表。

```c
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
```

- 例子2：对if的语义处理，我们根据对应语法解析并创建相应表达式

```c
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
```

### 核心数据类型

- 我们在语义分析中的主要动作为存储信息，推后计算，那么就要有相应数据结构存储对应信息。
- `SCP_Interpreter`——顶层设计，所有功能的基础

```c
/* SCP解释器 */
struct SCP_Interpreter_tag {
    MEM_Storage         interpreter_storage;    /* 解释器内存 */
    MEM_Storage         execute_storage;        /* 执行内存 */
    Variable            *variable;              /* 变量 */
    FunctionDefinition  *function_list;         /* 函数定义链表 */
    StatementList       *statement_list;        /* 语句链表 */
    int                 current_line_number;    /* 行号 */
};
```

- `expression`——表达式，基础运算单元

```c
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
```

### 计算模块

- 存储信息之后便是对应运算，因为是**无类型语言**，所以我们的运算**不仅要计算结果值，也要运算结果类型**（运算值类型在赋值时便进行猜测获取）

- 例子：计算一个二元表达式

```c
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
    /* 略去…… */
    /* 左边字符串且操作符为加的处理 */
    else if (left_val.type == SCP_STRING_VALUE && operator == ADD_EXPRESSION) {
        /* 略去…… */
    }
    /* 如果有任一边为NULL */
    else if (left_val.type == SCP_NULL_VALUE || right_val.type == SCP_NULL_VALUE) {
        result.type = SCP_BOOLEAN_VALUE;
        result.u.boolean_value = eval_binary_null(operator, &left_val, &right_val, left->line_number);
    } 
    /* 略去…… */
    return result;
}
```

- 可以看到，我们必须对每种可能的情况进行分析，以计算最后的表达式类型
- 这也是为什么**无类型语言速度一般会比强类型语言慢**

### debug模块

- 给面向解释器开发人员的`assert`和`panic`函数封装

  - 宏定义了默认变长参数函数`DBG_assert`和`DBG_panic`，可以直接在源代码中进行调用
  - 调用示例

    ```c
        /* 要求表达式cond.type == SCP_BOOLEAN_VALUE成立，否则报错并返回位置和信息 */
        DBG_assert(cond.type == SCP_BOOLEAN_VALUE, ("cond.type..%d", cond.type));

        /* 返回结果 */
        Assertion failure (cond.type == SCP_BOOLEAN_VALUE)   file:execute.c   line:103  cond.type..0
    ```

- 给语言使用编程人员做了报错模块，包括编译报错和运行时报错，可以猜测错误发生位置（如果猜测失败则返回-1），并附有错误信息。

  - 例子：

    ```c
    /* 写代码时不小心多打了几个字符 */
    print("10.0 % 8.0 = " + (10.0 % 8.0) + "\n");aas  /
    /* 编译报错返回结果，12为行号 */
    12:在(print)附近发生语法错误

    /* 调用未声明的函数 */
    gtestfunc();
    /* 运行报错返回，299为行号 */
    299:找不到函数(gtestfunc)。
    ```

### memory模块

- 对开辟、释放内存空间的封装，便于解释器开发人员直接使用
- 目前有这5种函数宏定义声明

```c
#define MEM_malloc(size) (MEM_malloc_func(__FILE__, __LINE__, size))

#define MEM_realloc(ptr, size) (MEM_realloc_func(__FILE__, __LINE__, ptr, size))

#define MEM_strdup(str) (MEM_strdup_func(__FILE__, __LINE__, str))

#define MEM_open_storage(page_size) (MEM_open_storage_func(__FILE__, __LINE__, page_size))

#define MEM_storage_malloc(storage, size) (MEM_storage_malloc_func(__FILE__, __LINE__, storage, size))
```

### sicpy原生函数与C语言函数预留接口

- sicpy原生函数如`print`、`fopen`、`fwrite`、`fread`、`fclose`
- 给扩展C语言原生函数预留了接口
- 接口示例：书写对应函数后，到add_native_functions处注册即可

```c
/* 新建scp原生函数集群 */
static void add_native_functions(SCP_Interpreter *inter)
{
    SCP_add_native_function(inter, "print", scp_nv_print_proc);
    SCP_add_native_function(inter, "fopen", scp_nv_fopen_proc);
    SCP_add_native_function(inter, "fclose", scp_nv_fclose_proc);
    SCP_add_native_function(inter, "fread", scp_nv_fread_proc);
    SCP_add_native_function(inter, "fwrite", scp_nv_fwrite_proc);
}
```

## 使用手册

### 编译及运行

1. 编译：win10在SCP文件夹下运行`make`进行编译，生成sicpy.exe（需要flex、bison、gcc环境）
2. 运行：在test文件夹下已有一个测试文件，运行`.\sicpy test/test.scp`执行程序，即可看到输出。

### 语言描述

1. 源文件编码：
   一般情况下，可以在win10环境下使用**UTF-8**进行编码。如果需要输出中文字符等，则需要使用**GBK**编码格式（和控制台输出格式相同）
2. 关键字

   ```c
   if else elif for function global return break null true false continue while
   ```

3. 注释：使用#进行注释
4. 标识符
   第一个字母是英文字母（大小写均可）或下划线
   第二个字符开始可以是英文字母，下划线或数字
5. 常量
   真假值常量，整数常量，实数常量，字符串常量，null常量
6. 运算符：

   ```c
   && || = == != > >= < <= + - * / % !
   ```

7. 分隔符：
   `() {} ; ,`
8. 隐式类型转换
   使用双目运算符和比较运算符时，如果左右两边类型不同，基于以下规则进行类型转换：
   - 只要一边为实数，另一边为整数则会转换为实数运算
   - 左边是字符串，右边是逻辑类型/整数类型/实数类型则右边会转化为字符串
9. global语句
    为了在函数内引用全局变量，必须加上global语句，减少不经意间对全局变量的修改
10. 函数定义
    使用`function`关键字对函数进行声明

### 输入输出样例

- 样例1：for循环，if、else

输入：

```c
for (i = 0; i < 5; i = i + 1) {     # for循环
    if (i == 0) {
        print("i = 0\n");
    } 
    elif (i == 1) {
        print("i = 1\n");
    }
    elif (i == 2) {
        print("i = 2\n");
    }
    else {
        print("i = else\n");
    }
}
```

输出：

```c
i = 0
i = 1
i = 2
i = else
i = else
```

- 局部变量与全局变量

输入：

```c
value = 10;
function func() {
    global value;
    value = 20;
}
function func2() {
    value = 30;
    print("value.." + value + "\n");    # 应该输出30
}
func();
func2();
print("value.." + value + "\n");        # 应该输出20，因为全局变量被func改变
```

输出：

```c
value..30
value..20
```

## 参考资料

- 《编译原理》（龙书）——（美） Alfred V. Aho
- 《现代编译原理-C语言描述》（虎书）——（美）Andrew W.Appel
- 《高级编译器设计与实现》（鲸书）——（美）Steven S.Muchnick
- 《自制编程语言》——（日）前桥和弥

# SCP/sicpy

English | [简体中文](README.zh-cn.md)

- [SCP/sicpy](#scpsicpy)
  - [Introduction](#introduction)
  - [Development Details](#development-details)
    - [Code Count](#code-count)
    - [Lexical Analysis](#lexical-analysis)
    - [Syntax Analysis](#syntax-analysis)
    - [Semantic Analysis](#semantic-analysis)
    - [Core Data Types](#core-data-types)
    - [Computation Module](#computation-module)
    - [Debug Module](#debug-module)
    - [Memory Module](#memory-module)
    - [Sicpy Native Functions and C Language Function Interface](#sicpy-native-functions-and-c-language-function-interface)
  - [User Manual](#user-manual)
    - [Compilation and Execution](#compilation-and-execution)
    - [Language Description](#language-description)
    - [Input and Output Examples](#input-and-output-examples)
  - [References](#references)

## Introduction

Summary: **SCP** stands for simple C and python, which I named as **sicpy**. Sicpy is an **untyped language** that integrates features of both C and Python, and is developed entirely in C. Here are some features of the language:

- Variables do not require type declaration, the compiler will automatically guess the type.
- Uses `if`, `elif`, and `else` in place of `switch`.
- Accessing global variables in local scope requires the use of **global**.
- Native string type with automatic garbage collection through **reference counting** and can be concatenated with **+**.
- Native sicpy functions such as `print`, `fopen`, `fwrite`, `fread`, `fclose`, with an interface reserved for calling C functions.
- A DBG module for interpreter developers (assert and panic) and an error-reporting module precise to the line number for programmers (runtime and compile errors).
- ...more features awaiting exploration.

## Development Details

### Code Count

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

### Lexical Analysis

- Global variable: Used to store the string stream processed by flex.

```c
#define STRING_ALLOC_SIZE       (256)   /* new buffersize when buffers run out*/
static char *flex_string_buffer = NULL;
static int flex_string_buffer_size = 0;
static int flex_string_buffer_alloc_size = 0;
```

- Key functions: Called in flex, facilitating interaction with global variables.

```c
/* Add a new character to the string */
void scp_add_character(int letter)
/* Clear the string buffer */
void scp_reset_string_buffer(void)
/* Terminate the string, appending \0 at the end */
char * scp_close_string(void)
```

- Flex states: Includes **INITIAL**, **COMMENT**, and **STRING**.
  - **INITIAL** is the default state, processing identifiers, parentheses, numbers, etc. Encounter `#` to initiate **COMMENT** mode, and `"` to initiate **STRING** mode.
  - **COMMENT** is for comments, where all characters are discarded. On encountering `\n`, it returns to **INITIAL**.
  - **STRING** is for processing strings, the aforementioned global variables and key functions are used here. Encounter `"` again to return to **INITIAL**.

- Example

```c
<INITIAL># {
    /* `#` means comment mode starts */
    BEGIN COMMENT;
}
<COMMENT>\n {
    /* Encountering a newline in a comment indicates the end of the comment and returns to the INITIAL state */
    increment_line_number();
    BEGIN INITIAL;
}
<INITIAL>\" {
    /* Matching the start of a string, set buffer to 0 */
    flex_string_buffer_size = 0;
    BEGIN STRING;
}
<STRING>\" {
    /* In string state, encountering " indicates the end of the string, and the entire string is added to the expression */
    Expression *expression = scp_alloc_expression(STRING_EXPRESSION);
    // scp_close_string() copys current string and adds `\0` in the end
    expression->u.string_value = scp_close_string();
    yylval.expression = expression;
    BEGIN INITIAL;     // back to INITIAL State
    return STRING_TOKEN;
}
```

### Syntax Analysis

- Core Data Structures:

```c
%union {
    char                *identifier;       /* Identifier */
    ParameterList       *parameter_list;   /* Parameter list */
    ArgumentList        *argument_list;    /* Argument list */
    Expression          *expression;       /* Expression */
    Statement           *statement;        /* Statement */
    StatementList       *statement_list;   /* List of statements */
    Block               *block;            /* Block, contains list of statements */
    Elif                *elif;             /* elif expression */
    IdentifierList      *identifier_list;  /* List of identifiers */
}
```

- **Recursive Grammar Design** is used for expression evaluation.

Note: The lower the priority, the higher the level in grammar design, and the later it gets calculated.

```c
expression -> logical_or_expression -> logical_and_expression -> equality_expression
-> relational_expression -> additive_expression -> multiplicative_expression 
-> unary_expression -> primary_expression
```

- `statment_list` — List of statements:
  
  - A statement is parsed as an expression followed by a semicolon, in the form: `expression SEMICOLON`. It consists of 7 types of statements.

    ```c
        | global_statement
        | if_statement
        | while_statement
        | for_statement
        | return_statement
        | break_statement
        | continue_statement
    ```

  - The list of statements chains statements together, forming an overall structure that facilitates invocation.

- `block` — Application of code blocks:

  - There are two types of blocks: one that contains a list of statements, such as {list of statements}: `LC statement_list RC`, and one that's an empty block, like {}: `LC RC`.
  - Once a block is declared, it can be flexibly used in function definitions, loops, etc.

- `argument_list` and `parameter_list` — Lists of arguments and parameters:

  - The list of arguments or parameters is composed of parameters.
  - If a single identifier or expression is passed, like `identifier` (parameter) or `expression` (argument), a corresponding list is created.
  - If a list already exists, they are linked.

- `function` — Function definition and parameter passing. There are two grammar patterns depending on whether parameters are passed:
  - `function identifier() {}`.
  - `function identifier(parameter_list) {}`.

- `if_statement` — If statements. There are four parsing modes for if statements:
    1. A single if, in the form `IF LP expression RP block`.
    2. If + else, in the form `IF LP expression RP block ELSE block`.
    3. If + list of elif, in the form `IF LP expression RP block elif_list`.
    4. If + list of elif + else, in the form `IF LP expression RP block elif_list ELSE block`.

- `elif` — Implementation of elif expressions:

  - Elif is essentially the same as if statements, where the expression is stored in the corresponding data structure.
  - However, the difference with elif is that it can extend indefinitely (an if can have countless elifs), so we need to use `elif_list` to chain statements.
  - The chaining logic is the same as the list of statements and the list of parameters.

### Semantic Analysis

- The basic idea of semantic analysis is **to allocate memory space for expressions and store them in the corresponding data structures**. Calculations or processes are then conducted at the appropriate time.
- Why not calculate directly? Because we are an interpreted language. We need to **infer the type** based on the context, and direct calculation can cause **false short-circuiting**.
  - What is false short-circuiting? For example, if we directly perform an addition calculation when analyzing `additive_expression`, due to the order of logical computation, by the time we pass it to `logical_and_expression LOGICAL_AND equality_expression`, our left and right expressions **have already been calculated**. Carrying out a logical short-circuit operation at this point is actually meaningless.

- Example: For semantic processing of the block, we allocate memory and store the corresponding list of statements.

```c
/* code block */
block: LC statement_list RC{
            /* eg: {a=2; b=3;} */
            Block *block = scp_malloc(sizeof(Block));
            block->statement_list = $2;
            $$ = block;
        }
        | LC RC {
            /* empty code block */
            Block *block = scp_malloc(sizeof(Block));
            block->statement_list = NULL;
            $$ = block;
        };
```

- Example 2: For semantic processing of the if statement, we parse the corresponding grammar and create the relevant expression.

```c
/* create if statement */
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

/* if statement */
if_statement: IF LP expression RP block {
            /* eg: if(3<5){} */
            $$ = scp_create_if_statement($3, $5, NULL, NULL);
        }
        | IF LP expression RP block ELSE block {
            /* eg: if(2>4){} else{} */
            $$ = scp_create_if_statement($3, $5, NULL, $7);
        }
        | IF LP expression RP block elif_list {
            /* eg: if(2>4){} elif{} elif{} */
            $$ = scp_create_if_statement($3, $5, $6, NULL);
        }
        | IF LP expression RP block elif_list ELSE block {
            /* eg: if(2>4){} elif{} elif{} else{} */
            $$ = scp_create_if_statement($3, $5, $6, $8);
        };
```

### Core Data Types

- The main actions in our semantic analysis are to store information and defer calculations. Therefore, we need corresponding data structures to store the relevant information.
- `SCP_Interpreter` — Top-level design, the foundation of all functions

```c
/* SCP Interpreter */
struct SCP_Interpreter_tag {
    MEM_Storage         interpreter_storage;    /* Interpreter memory */
    MEM_Storage         execute_storage;        /* Execution memory */
    Variable            *variable;              /* Variable */
    FunctionDefinition  *function_list;         /* List of function definitions */
    StatementList       *statement_list;        /* Statement list */
    int                 current_line_number;    /* Line number */
};
```

- **Expression**, the basic computational unit

```c
/* Expression */
struct Expression_tag {
    ExpressionType type;
    int line_number;
    union {
        SCP_Boolean             boolean_value;              /* Boolean value */
        int                     int_value;                  /* int value */
        double                  double_value;               /* double value */
        char                    *string_value;              /* string value */
        char                    *identifier;                /* Identifier */
        AssignExpression        assign_expression;          /* Assignment expression */
        BinaryExpression        binary_expression;          /* Binary expression */
        Expression              *minus_expression;          /* Negative value expression */
        FunctionCallExpression  function_call_expression;   /* Function call expression */
    } u;
};
```

### Computation Module

- After storing the information, the corresponding operations are performed. Since it is a **typeless language**, our operations **need to compute not only the result value but also the result type** (the type of the operation value is guessed during assignment).

- Example: Computing a binary expression

```c
/* Calculate the expression, passing in the interpreter, current environment, operator, and left and right expressions for computation */
SCP_Value scp_eval_binary_expression(SCP_Interpreter *inter, LocalEnvironment *env,
                           ExpressionType operator, Expression *left, Expression *right)
{
    SCP_Value   left_val = eval_expression(inter, env, left);
    SCP_Value   right_val = eval_expression(inter, env, right);
    SCP_Value   result;

    /* Computation where both left and right are of type int */
    if (left_val.type == SCP_INT_VALUE && right_val.type == SCP_INT_VALUE) {
        eval_binary_int(inter, operator, left_val.u.int_value,
                        right_val.u.int_value, &result, left->line_number);
    }
    /* Computation where both left and right are of type double */
    else if (left_val.type == SCP_DOUBLE_VALUE && right_val.type == SCP_DOUBLE_VALUE) {
        eval_binary_double(operator, left_val.u.double_value,
                            right_val.u.double_value, &result, left->line_number);

    }
    /* Computation where the left is int and the right is double */
    else if (left_val.type == SCP_INT_VALUE && right_val.type == SCP_DOUBLE_VALUE) {
        left_val.u.double_value = left_val.u.int_value;     /* datatype cast */
        eval_binary_double(operator, left_val.u.double_value, right_val.u.double_value,
                           &result, left->line_number);
    }
    /* skip…… */
    /* Handling the left side as a string and the operator is addition */
    else if (left_val.type == SCP_STRING_VALUE && operator == ADD_EXPRESSION) {
        /* skip…… */
    }
    /* If either side is NULL */
    else if (left_val.type == SCP_NULL_VALUE || right_val.type == SCP_NULL_VALUE) {
        result.type = SCP_BOOLEAN_VALUE;
        result.u.boolean_value = eval_binary_null(operator, &left_val, &right_val, left->line_number);
    } 
    /* skip…… */
    return result;
}
```

- As can be seen, we have to analyze each possible scenario to compute the final expression type.
- This is also why **typeless languages are generally slower than strongly typed languages**.

### Debug Module

- An encapsulation of `assert` and `panic` functions for interpreter developers.

  - Macro-defined default variadic functions `DBG_assert` and `DBG_panic` can be directly called in the source code.
  - Usage example:

    ```c
        /* Asserts that the expression cond.type == SCP_BOOLEAN_VALUE is true, otherwise it raises an error and returns the position and information. */
        DBG_assert(cond.type == SCP_BOOLEAN_VALUE, ("cond.type..%d", cond.type));

        /* return results */
        Assertion failure (cond.type == SCP_BOOLEAN_VALUE)   file:execute.c   line:103  cond.type..0
    ```

- An **error-report** module is available for language programmers, including compilation errors and runtime errors. It can guess the error location (returns -1 if the guess fails) and provides an error message.

  - Example：

    ```c
    /* Mistakenly typing extra characters in the code */
    print("10.0 % 8.0 = " + (10.0 % 8.0) + "\n");aas  /
    /* Compilation error output with 12 as the line number */
    12: Syntax error near (print).

    /* Calling an undeclared function */
    gtestfunc();
    /* Runtime error with 299 as the line number */
    299: Function (gtestfunc) not found.
    ```

### Memory Module

- Encapsulation for memory allocation and deallocation, for interpreter developers.
- Currently, there are these 5 function macro definitions.

```c
#define MEM_malloc(size) (MEM_malloc_func(__FILE__, __LINE__, size))

#define MEM_realloc(ptr, size) (MEM_realloc_func(__FILE__, __LINE__, ptr, size))

#define MEM_strdup(str) (MEM_strdup_func(__FILE__, __LINE__, str))

#define MEM_open_storage(page_size) (MEM_open_storage_func(__FILE__, __LINE__, page_size))

#define MEM_storage_malloc(storage, size) (MEM_storage_malloc_func(__FILE__, __LINE__, storage, size))
```

### Sicpy Native Functions and C Language Function Interface

- Sicpy native functions such as `print`, `fopen`, `fwrite`, `fread`, and `fclose`.
- An interface is reserved for extending native C language functions.
- Interface example: After writing the corresponding function, register it at the `add_native_functions` location.

```c
/* add scp native functions groups */
static void add_native_functions(SCP_Interpreter *inter)
{
    SCP_add_native_function(inter, "print", scp_nv_print_proc);
    SCP_add_native_function(inter, "fopen", scp_nv_fopen_proc);
    SCP_add_native_function(inter, "fclose", scp_nv_fclose_proc);
    SCP_add_native_function(inter, "fread", scp_nv_fread_proc);
    SCP_add_native_function(inter, "fwrite", scp_nv_fwrite_proc);
}
```

## User Manual

### Compilation and Execution

1. Compilation: On Windows 10, run `make` in the SCP folder to compile and generate `sicpy.exe` (requires **flex, bison, and gcc** environment).
2. Execution: A test file is already present in the `test` folder. Run `.\sicpy test/test.scp` to execute the program and see the output.

### Language Description

1. Source File Encoding:
   Generally, you can use **UTF-8** encoding on Windows 10. If you need to output Chinese characters, use **GBK** encoding format (same as the console output format).
2. Keywords:

   ```c
   if else elif for function global return break null true false continue while
   ```

3. Comments: Use `#` for comments.
4. Identifiers:
   The first letter should be an English alphabet (uppercase or lowercase) or an underscore.
   From the second character onwards, it can be an English alphabet, underscore, or a number.
5. Constants:
   Boolean values, integer constants, real number constants, string constants, null constants.
6. Operators:

   ```c
   && || = == != > >= < <= + - * / % !
   ```

7. Delimiters:
   `() {} ; ,`
8. Implicit Type Conversion:
   When using binary operators and comparison operators, if the types on both sides are different, type conversion is based on the following rules:
   - If one side is a real number and the other is an integer, it will be converted to real number operations.
   - If the left is a string and the right is a boolean type/int type/real type, the right will be converted to a string.
9. Global Statement:
   To reference a global variable inside a function, you must use the `global` statement to avoid unintended modifications to global variables.
10. Function Definitions:
   Use the `function` keyword to declare a function.

### Input and Output Examples

- Example 1: for loop, if, else

Input:

```c
for (i = 0; i < 5; i = i + 1) {
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

Output：

```c
i = 0
i = 1
i = 2
i = else
i = else
```

- local and global variable

Input:

```c
value = 10;
function func() {
    global value;
    value = 20;
}
function func2() {
    value = 30;
    print("value.." + value + "\n");    # output 30
}
func();
func2();
print("value.." + value + "\n");        # output 20, because the global variable was modified by the function.
```

output：

```c
value..30
value..20
```

## References

- "Compilers: Principles, Techniques, and Tools" (Dragon Book) — Alfred V. Aho, et al.
- "Modern Compiler Implementation in C" (Tiger Book) — Andrew W. Appel
- "Advanced Compiler Design and Implementation" (Whale Book) — Steven S. Muchnick
- "Creating Your Own Programming Language" — Kazuya Maehashi (Japanese)

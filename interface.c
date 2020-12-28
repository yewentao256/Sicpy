#include "MEM.h"
#include "DBG.h"
#define GLOBAL_VARIABLE_DEFINE
#include "sicpy.h"

/* 新建scp原生函数集群 */
static void add_native_functions(SCP_Interpreter *inter)
{
    SCP_add_native_function(inter, "print", scp_nv_print_proc);
    SCP_add_native_function(inter, "fopen", scp_nv_fopen_proc);
    SCP_add_native_function(inter, "fclose", scp_nv_fclose_proc);
    SCP_add_native_function(inter, "fread", scp_nv_fread_proc);
    SCP_add_native_function(inter, "fwrite", scp_nv_fwrite_proc);
}

/* 创建解释器 */
SCP_Interpreter * SCP_create_interpreter(void)
{
    MEM_Storage storage = MEM_open_storage(0);
    SCP_Interpreter *interpreter = MEM_storage_malloc(storage, sizeof(struct SCP_Interpreter_tag));
    interpreter->interpreter_storage = storage;
    interpreter->execute_storage = NULL;
    interpreter->variable = NULL;
    interpreter->function_list = NULL;
    interpreter->statement_list = NULL;
    interpreter->current_line_number = 1;
    scp_set_current_interpreter(interpreter);
    add_native_functions(interpreter);

    return interpreter;
}

/* 进行编译 */
void SCP_compile(SCP_Interpreter *interpreter, FILE *fp)
{
    extern int yyparse(void);
    extern FILE *yyin;
    scp_set_current_interpreter(interpreter);

    yyin = fp;
    if (yyparse()) {
        fprintf(stderr, "Error ! Error ! Error !\n");
        exit(1);
    }
    scp_reset_string_buffer();
}

/* 进行解释 */
void SCP_interpret(SCP_Interpreter *interpreter)
{
    interpreter->execute_storage = MEM_open_storage(0);
    scp_add_std_fp(interpreter);
    scp_execute_statement_list(interpreter, NULL, interpreter->statement_list);
}


/* 释放解释器所有字串 */
static void release_global_strings(SCP_Interpreter *interpreter) {
    while (interpreter->variable) {
        Variable *temp = interpreter->variable;
        interpreter->variable = temp->next;
        if (temp->value.type == SCP_STRING_VALUE) {
            scp_release_string(temp->value.u.string_value);
        }
    }
}

/* 销毁解释器 */
void SCP_dispose_interpreter(SCP_Interpreter *interpreter)
{
    release_global_strings(interpreter);

    if (interpreter->execute_storage) {
        MEM_dispose_storage(interpreter->execute_storage);
    }

    MEM_dispose_storage(interpreter->interpreter_storage);
}

/* 新增scp原生函数 */
void SCP_add_native_function(SCP_Interpreter *interpreter, char *name, SCP_NativeFunctionProc *proc)
{
    FunctionDefinition *fd = scp_malloc(sizeof(FunctionDefinition));
    fd->name = name;
    fd->type = NATIVE_FUNCTION_DEFINITION;
    fd->u.native_f.proc = proc;     /* 函数指针 */
    /* 头插法加入解释器函数表 */
    fd->next = interpreter->function_list;
    interpreter->function_list = fd;
}

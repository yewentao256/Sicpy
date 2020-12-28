#include <stdio.h>
#include <string.h>
#include "MEM.h"
#include "DBG.h"
#include "sicpy.h"

static char* st_native_lib_info = "sicpy.lang.file";

/* SCP原生print函数 */
SCP_Value scp_nv_print_proc(SCP_Interpreter *interpreter, int arg_count, SCP_Value *args)
{
    SCP_Value value;
    value.type = SCP_NULL_VALUE;

    /* 参数少于1或大于1报错 */
    if (arg_count < 1) {
        scp_runtime_error(-1, ARGUMENT_TOO_FEW_ERR, MESSAGE_ARGUMENT_END);
    } 
    else if (arg_count > 1) {
        scp_runtime_error(-1, ARGUMENT_TOO_MANY_ERR, MESSAGE_ARGUMENT_END);
    }

    /* 根据参数类型print */
    switch (args[0].type) {
    case SCP_BOOLEAN_VALUE:
        if (args[0].u.boolean_value) {
            printf("true");
        } else {
            printf("false");
        }
        break;
    case SCP_INT_VALUE:
        printf("%d", args[0].u.int_value);
        break;
    case SCP_DOUBLE_VALUE:
        printf("%f", args[0].u.double_value);
        break;
    case SCP_STRING_VALUE:
        printf("%s", args[0].u.string_value->string);
        break;
    case SCP_NATIVE_POINTER_VALUE:
        printf("(%s:%p)", args[0].u.native_pointer.info, args[0].u.native_pointer.pointer);
        break;
    case SCP_NULL_VALUE:
        printf("null");
        break;
    }

    return value;
}

/* scp原生打开文件函数 */
SCP_Value scp_nv_fopen_proc(SCP_Interpreter *interpreter, int arg_count, SCP_Value *args)
{
    SCP_Value value;
    /* 参数应该是2个，超过或者少于都报错 */
    if (arg_count < 2) {
        scp_runtime_error(-1, ARGUMENT_TOO_FEW_ERR, MESSAGE_ARGUMENT_END);
    } 
    else if (arg_count > 2) {
        scp_runtime_error(-1, ARGUMENT_TOO_MANY_ERR, MESSAGE_ARGUMENT_END);
    }
    /* 如果参数不是string类型，报错 */
    if (args[0].type != SCP_STRING_VALUE || args[1].type != SCP_STRING_VALUE) {
        scp_runtime_error(-1, FOPEN_ARGUMENT_TYPE_ERR, MESSAGE_ARGUMENT_END);
    }
    
    /* 底层使用C语言的fopen */
    FILE *fp = fopen(args[0].u.string_value->string, args[1].u.string_value->string);
    if (fp == NULL) {
        value.type = SCP_NULL_VALUE;
    }
    else {
        value.type = SCP_NATIVE_POINTER_VALUE;
        value.u.native_pointer.info = st_native_lib_info;
        value.u.native_pointer.pointer = fp;
    }

    return value;
}

/* 检测指针信息 */
static SCP_Boolean check_native_pointer(SCP_Value *value)
{
    return value->u.native_pointer.info == st_native_lib_info;
}

/* scp原生关闭文件函数 */
SCP_Value scp_nv_fclose_proc(SCP_Interpreter *interpreter, int arg_count, SCP_Value *args)
{
    SCP_Value value;

    value.type = SCP_NULL_VALUE;
    /* 参数应该为1个，否则报错 */
    if (arg_count < 1) {
        scp_runtime_error(-1, ARGUMENT_TOO_FEW_ERR, MESSAGE_ARGUMENT_END);
    }
    else if (arg_count > 1) {
        scp_runtime_error(-1, ARGUMENT_TOO_MANY_ERR, MESSAGE_ARGUMENT_END);
    }
    /* 参数类型非指针或检查信息不对，则报错 */
    if (args[0].type != SCP_NATIVE_POINTER_VALUE || !check_native_pointer(&args[0])) {
        scp_runtime_error(-1, FCLOSE_ARGUMENT_TYPE_ERR, MESSAGE_ARGUMENT_END);
    }
    FILE *fp = args[0].u.native_pointer.pointer;
    fclose(fp);

    return value;
}

/* SCP原生读取文件函数 */
SCP_Value scp_nv_fread_proc(SCP_Interpreter *interpreter, int arg_count, SCP_Value *args)
{
    /* 参数数量应为1个，否则报错 */
    if (arg_count < 1) {
        scp_runtime_error(-1, ARGUMENT_TOO_FEW_ERR, MESSAGE_ARGUMENT_END);
    }
    else if (arg_count > 1) {
        scp_runtime_error(-1, ARGUMENT_TOO_MANY_ERR, MESSAGE_ARGUMENT_END);
    }
    /* 参数类型非指针或检查信息不对，则报错 */
    if (args[0].type != SCP_NATIVE_POINTER_VALUE || !check_native_pointer(&args[0])) {
        scp_runtime_error(-1, FREAD_ARGUMENT_TYPE_ERR, MESSAGE_ARGUMENT_END);
    }

    SCP_Value value;
    char buf[LINE_BUF_SIZE];
    FILE *fp = args[0].u.native_pointer.pointer;
    char *read_buf = NULL;
    int read_len = 0;

    /* 读取数据至readbuf中，富翁式编程 */
    while (fgets(buf, LINE_BUF_SIZE, fp)) {
        int new_len = read_len + strlen(buf);
        read_buf = MEM_realloc(read_buf, new_len + 1);
        /* 如果已读长度为0，则buf复制至read_buf，否则使用连接 */
        if (read_len == 0) {
            strcpy(read_buf, buf);
        }
        else {
            strcat(read_buf, buf);
        }
        read_len = new_len;
        /* 换行符退出 */
        if (read_buf[read_len-1] == '\n')
            break;
    }
    /* 读到数据，创建字符串，否则返回空 */
    if (read_len > 0) {
        value.type = SCP_STRING_VALUE;
        value.u.string_value = scp_create_sicpy_string(read_buf);
    }
    else {
        value.type = SCP_NULL_VALUE;
    }
    return value;
}

/* SCP原生写函数 */
SCP_Value scp_nv_fwrite_proc(SCP_Interpreter *interpreter, int arg_count, SCP_Value *args)
{
    SCP_Value value;

    value.type = SCP_NULL_VALUE;
    if (arg_count < 2) {
        scp_runtime_error(-1, ARGUMENT_TOO_FEW_ERR, MESSAGE_ARGUMENT_END);
    }
    else if (arg_count > 2) {
        scp_runtime_error(-1, ARGUMENT_TOO_MANY_ERR, MESSAGE_ARGUMENT_END);
    }
    if (args[0].type != SCP_STRING_VALUE || (args[1].type != SCP_NATIVE_POINTER_VALUE
            || !check_native_pointer(&args[1]))) {
        scp_runtime_error(-1, FWRITE_ARGUMENT_TYPE_ERR, MESSAGE_ARGUMENT_END);
    }
    FILE *fp = args[1].u.native_pointer.pointer;
    fputs(args[0].u.string_value->string, fp);
    return value;
}

/* 添加标准指针 */
void scp_add_std_fp(SCP_Interpreter *inter)
{
    SCP_Value fp_value;

    fp_value.type = SCP_NATIVE_POINTER_VALUE;
    fp_value.u.native_pointer.info = st_native_lib_info;

    /* STDIN,STDOUT,STDERR作为全局变量 */
    fp_value.u.native_pointer.pointer = stdin;
    scp_add_global_variable(inter, "STDIN", &fp_value);
    fp_value.u.native_pointer.pointer = stdout;
    scp_add_global_variable(inter, "STDOUT", &fp_value);
    fp_value.u.native_pointer.pointer = stderr;
    scp_add_global_variable(inter, "STDERR", &fp_value);
}

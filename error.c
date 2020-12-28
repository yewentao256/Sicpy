#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <assert.h>
#include "MEM.h"
#include "DBG.h"
#include "sicpy.h"

extern char *yytext;


/* 编译报错信息 */
char* scp_compile_error_message_format[] = {
    "在($(token))附近发生语法错误",
    "不正确的字符($(bad_char))",
    "函数名重复($(name))",
};

/* 运行时报错信息 */
char* scp_runtime_error_message_format[] = {
    "找不到变量($(name))。",
    "找不到函数($(name))。",
    "传入的参数数量多于函数定义。",
    "传入的参数数量少于函数定义。",
    "条件表达式的值必须是boolean型。",
    "减法运算的操作数必须是数值类型。",
    "双目操作符$(operator)的操作数类型不正确。",
    "$(operator)操作符不能用于boolean型。",
    "找不到对应文件，请为fopen()函数传入文件的路径和打开方式（两者都是字符串类型的）。",
    "请为fclose()函数传入文件指针。",
    "请为fread()函数传入文件指针。",
    "请为fwrite()函数传入文件指针和字符串。",
    "null只能用于运算符 == 和 !=(不能进行$(operator)操作)。",
    "除0错误，请检查",
    "全局变量$(name)不存在。",
    "不能在函数外使用global语句。",
    "运算符$(operator)不能用于字符串类型。",
};

/* 字符数组指针定义为字串 */
typedef struct {
    char        *string;
} VString;

/* 统计字串长度 */
int my_strlen(char *str)
{
    /* 如果不慎传入空指针，则改变为0（strlen遇到空指针不能处理） */
    if (str == NULL) {
        return 0;
    }
    return strlen(str);
}

/* 添加字串 */
static void add_string(VString *message, char *str)
{

    int old_len = my_strlen(message->string);
    int new_size = old_len + strlen(str) + 1;
    message->string = MEM_realloc(message->string, new_size);
    strcpy(&message->string[old_len], str);
}

/* 添加字符 */
static void add_character(VString *message, int ch)
{
    int current_len = my_strlen(message->string);
    message->string = MEM_realloc(message->string, current_len + 2);
    message->string[current_len] = ch;
    message->string[current_len+1] = '\0';
}

/* 处理变长实参，全部存入args数组中 */
static void create_message_args(MessageArgument *arg_list, va_list ap)
{
    int index = 0;
    MessageArgumentType type;
    
    /* 遍历参数列表，直到设定的参数末尾MESSAGE_ARGUMENT_END，va_arg获取指定类型的当前参数 */
    while ((type = va_arg(ap, MessageArgumentType)) != MESSAGE_ARGUMENT_END) {
        arg_list[index].type = type;
        arg_list[index].name = va_arg(ap, char*);
        switch (type) {
        case INT_MESSAGE_ARGUMENT:
            arg_list[index].u.int_val = va_arg(ap, int);
            break;
        case DOUBLE_MESSAGE_ARGUMENT:
            arg_list[index].u.double_val = va_arg(ap, double);
            break;
        case STRING_MESSAGE_ARGUMENT:
            arg_list[index].u.string_val = va_arg(ap, char*);
            break;
        case POINTER_MESSAGE_ARGUMENT:
            arg_list[index].u.pointer_val = va_arg(ap, void*);
            break;
        case CHARACTER_MESSAGE_ARGUMENT:
            arg_list[index].u.character_val = va_arg(ap, int);
            break;
        /* 以下是不该出现的情况，使用assert(0)报错 */
        case MESSAGE_ARGUMENT_END:
        default:
            assert(0);
        }
        index++;
        assert(index < MESSAGE_ARGUMENT_MAX);
    }
}

/* 在实参列表中找到指定参数名，赋给当前实参 */
static MessageArgument search_argument(MessageArgument *arg_list, char *arg_name)
{
    int i;

    for (i = 0; arg_list[i].type != MESSAGE_ARGUMENT_END; i++) {
        /* 如果名称相同 */
        if (!strcmp(arg_list[i].name, arg_name)) {
            return arg_list[i];
        }
    }
    assert(0);
}

/* 格式化报错信息 */
static void format_message(char *format, VString *message, va_list ap)
{
    int i;
    char        buf[LINE_BUF_SIZE];
    int         arg_name_index;
    char        arg_name[LINE_BUF_SIZE];
    MessageArgument arg_list[MESSAGE_ARGUMENT_MAX];     /* 变长实参信息列表 */

    /* 变长实参信息存入arg_list中 */
    create_message_args(arg_list, ap);

    /* 遍历字串，解析$ */
    for (i = 0; format[i] != '\0'; i++) {
        if (format[i] != '$') {
            add_character(message, format[i]);
            continue;
        }
        assert(format[i+1] == '(');
        i += 2;
        /* 开始解析$的参数 */
        for (arg_name_index = 0; format[i] != ')'; arg_name_index++, i++) {
            arg_name[arg_name_index] = format[i];
        }
        arg_name[arg_name_index] = '\0';
        assert(format[i] == ')');

        /* 搜索实参列表，找到当前实参 */
        MessageArgument cur_arg = search_argument(arg_list, arg_name);

        /* 根据实参类型，写入message字串 */
        switch (cur_arg.type) {
        case INT_MESSAGE_ARGUMENT:
            sprintf(buf, "%d", cur_arg.u.int_val);
            add_string(message, buf);
            break;
        case DOUBLE_MESSAGE_ARGUMENT:
            sprintf(buf, "%f", cur_arg.u.double_val);
            add_string(message, buf);
            break;
        case STRING_MESSAGE_ARGUMENT:
            strcpy(buf, cur_arg.u.string_val);
            add_string(message, cur_arg.u.string_val);
            break;
        case POINTER_MESSAGE_ARGUMENT:
            sprintf(buf, "%p", cur_arg.u.pointer_val);
            add_string(message, buf);
            break;
        case CHARACTER_MESSAGE_ARGUMENT:
            sprintf(buf, "%c", cur_arg.u.character_val);
            add_string(message, buf);
            break;
        case MESSAGE_ARGUMENT_END:
        default:
            assert(0);
        }
    }
}


/* 编译型错误 */
void scp_compile_error(CompileError id, ...)
{
    /* 获取不确定参数的列表 */
    va_list     ap;
    VString     message;

    va_start(ap, id);   /* 根据第一个参数地址找到填充参数列表 */
    int line_number = scp_get_interpreter()->current_line_number;
    message.string = NULL;
    format_message(scp_compile_error_message_format[id], &message, ap);
    fprintf(stderr, "%3d:%s\n", line_number, message.string);
    va_end(ap);         /* 释放变长实参列表 */

    exit(1);
}

/* 运行时错误 */
void scp_runtime_error(int line_number, RuntimeError id, ...)
{
    va_list ap;
    VString message;

    va_start(ap, id);
    message.string = NULL;
    format_message(scp_runtime_error_message_format[id], &message, ap);
    fprintf(stderr, "%3d:%s\n", line_number, message.string);
    va_end(ap);

    exit(1);
}

/* 语法分析报错 */
int yyerror(char const *str)
{
    char *near_token;

    if (yytext[0] == '\0') {
        near_token = "EOF";
    } else {
        near_token = yytext;
    }
    scp_compile_error(PARSE_ERR, STRING_MESSAGE_ARGUMENT, "token", near_token, MESSAGE_ARGUMENT_END);
    return 0;
}

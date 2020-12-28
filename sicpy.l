%{
#include <stdio.h>
#include <string.h>
#include "DBG.h"
#include "MEM.h"
#include "sicpy.h"
#include "y.tab.h"

#define STRING_ALLOC_SIZE       (256)   /* 每次buffer不够，新增的buffersize */
static char *flex_string_buffer = NULL;
static int flex_string_buffer_size = 0;
static int flex_string_buffer_alloc_size = 0;

/* 给字符串添加一个新字符 */
void scp_add_character(int letter)
{
    if (flex_string_buffer_size == flex_string_buffer_alloc_size) {
        flex_string_buffer_alloc_size += STRING_ALLOC_SIZE;
        flex_string_buffer = MEM_realloc(flex_string_buffer, flex_string_buffer_alloc_size);
    }
    flex_string_buffer[flex_string_buffer_size] = letter;
    flex_string_buffer_size++;
}

/* 清空字串缓存 */
void scp_reset_string_buffer(void)
{
    MEM_free(flex_string_buffer);
    flex_string_buffer = NULL;
    flex_string_buffer_size = 0;
    flex_string_buffer_alloc_size = 0;
}

/* 关闭字符串，在字符串末尾加上\0 */
char * scp_close_string(void)
{
    char *new_str = scp_malloc(flex_string_buffer_size + 1);
    memcpy(new_str, flex_string_buffer, flex_string_buffer_size);
    new_str[flex_string_buffer_size] = '\0';
    return new_str;
}

int yywrap(void)
{
    return 1;
}

/* 行数+=1 */
static void increment_line_number(void)
{
    // 当前解释器对应行数+1, scp_get_interpreter为定义的获取当前解释器的函数
    scp_get_interpreter()->current_line_number++;
}
%}

%start COMMENT STRING
%%
<INITIAL>"function"     return FUNCTION;
<INITIAL>"if"           return IF;
<INITIAL>"else"         return ELSE;
<INITIAL>"elif"         return ELIF;
<INITIAL>"while"        return WHILE;
<INITIAL>"for"          return FOR;
<INITIAL>"return"       return RETURN_T;
<INITIAL>"break"        return BREAK;
<INITIAL>"continue"     return CONTINUE;
<INITIAL>"null"         return NULL_T;
<INITIAL>"true"         return TRUE_T;
<INITIAL>"false"        return FALSE_T;
<INITIAL>"global"       return GLOBAL_T;
<INITIAL>"("            return LP;
<INITIAL>")"            return RP;
<INITIAL>"{"            return LC;
<INITIAL>"}"            return RC;
<INITIAL>";"            return SEMICOLON;
<INITIAL>","            return COMMA;
<INITIAL>"&&"           return LOGICAL_AND;
<INITIAL>"||"           return LOGICAL_OR;
<INITIAL>"="            return ASSIGN;
<INITIAL>"=="           return EQ;
<INITIAL>"!="           return NE;
<INITIAL>">"            return GT;
<INITIAL>">="           return GE;
<INITIAL>"<"            return LT;
<INITIAL>"<="           return LE;
<INITIAL>"+"            return ADD;
<INITIAL>"-"            return SUB;
<INITIAL>"*"            return MUL;
<INITIAL>"/"            return DIV;
<INITIAL>"%"            return MOD;

<INITIAL>[A-Za-z_][A-Za-z_0-9]* {       
    /* 匹配到标识符，创建identifier并返回 */
    char *new_str = scp_malloc(strlen(yytext) + 1);
    strcpy(new_str, yytext);
    yylval.identifier = new_str;
    return IDENTIFIER;
}

<INITIAL>([1-9][0-9]*)|"0" {        
    /* 匹配int */
    // scp_alloc_expression预计为根据传入类型创建对应表达式
    Expression *expression = scp_alloc_expression(INT_EXPRESSION);
    sscanf(yytext, "%d", &expression->u.int_value);
    yylval.expression = expression;
    return INT_TOKEN;
}

<INITIAL>[0-9]+\.[0-9]+ {
    /* 匹配double */
    Expression  *expression = scp_alloc_expression(DOUBLE_EXPRESSION);
    sscanf(yytext, "%lf", &expression->u.double_value);
    yylval.expression = expression;
    return DOUBLE_TOKEN;
}

<INITIAL>\" {
    /* 匹配字符串的开始，缓冲区设为0 */
    flex_string_buffer_size = 0;
    BEGIN STRING;
}

<INITIAL>[ \t] {
    /* 空格和换挡不处理 */
}

<INITIAL>\n {
    /* 换行符行数自增 */
    increment_line_number();
}

<INITIAL># {
    /* #意味着开始注释模式 */
    BEGIN COMMENT;
}

<INITIAL>. {
    /* 到这里仍然没有任何匹配则报错 */
    char buf[LINE_BUF_SIZE];

    if (isprint(yytext[0])) {
        buf[0] = yytext[0];
        buf[1] = '\0';
    } else {
        //sprintf(buf, "0x%02x", (unsigned char)yytext[0]);
    }

    /* 编译报错函数 */
    scp_compile_error(CHARACTER_INVALID_ERR, STRING_MESSAGE_ARGUMENT, "bad_char", buf,
                      MESSAGE_ARGUMENT_END);
}

<COMMENT>\n {
    /* 在注释中遇到换行符，说明注释结束，并开始初始状态 */
    increment_line_number();
    BEGIN INITIAL;
}

<COMMENT>. {
    /* 注释中的任何字符不做任何处理 */
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

<STRING>\n {
    /* 字符串状态，遇到换行符仍然行号自增 */
    scp_add_character('\n');
    increment_line_number();
}
<STRING>\\\" {
    /* 字符串状态，遇到\\\"认为是添加一个" */
    scp_add_character('"');
}

<STRING>\\n {
    /* 字符串状态，遇到\\n认为是\n */
    scp_add_character('\n');
} 


<STRING>\\t {
    /* 字符串状态，遇到\\t认为是\t */
    scp_add_character('\t');
}

<STRING>\\\\ {
    /* 字符串状态，遇到\\\\认为是添加两个\ */
    scp_add_character('\\');
}

<STRING>. {
    /* 字符串状态，遇到任何字符添加到字符串缓冲区中 */
    scp_add_character(yytext[0]);
}
%%
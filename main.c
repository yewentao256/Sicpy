#include <stdio.h>
#include "sicpy.h"
#include "MEM.h"

/* main函数 */
int main(int argc, char **argv)
{

    if (argc != 2) {
        fprintf(stderr, "usage:%s filename", argv[0]);
        exit(1);
    }

    /* 打开代码文件 */
    FILE *fp = fopen(argv[1], "r");
    if (fp == NULL) {
        fprintf(stderr, "%s not found.\n", argv[1]);
        exit(1);
    }
    /* 新建解释器，编译、解释、销毁 */
    SCP_Interpreter *interpreter = SCP_create_interpreter();
    SCP_compile(interpreter, fp);
    SCP_interpret(interpreter);
    SCP_dispose_interpreter(interpreter);


    return 0;
}

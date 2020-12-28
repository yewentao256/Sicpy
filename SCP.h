#ifndef PUBLIC_SCP_H_INCLUDED
#define PUBLIC_SCP_H_INCLUDED
#include <stdio.h>


typedef struct SCP_Interpreter_tag SCP_Interpreter;


SCP_Interpreter *SCP_create_interpreter(void);
void SCP_compile(SCP_Interpreter *interpreter, FILE *fp);
void SCP_interpret(SCP_Interpreter *interpreter);
void SCP_dispose_interpreter(SCP_Interpreter *interpreter);

#endif /* PUBLIC_SCP_H_INCLUDED */

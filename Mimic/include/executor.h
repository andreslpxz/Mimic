#ifndef EXECUTOR_H
#define EXECUTOR_H

#include "cpu.h"

void execute(CPUState *cpu, uint64_t stack_base, uint64_t stack_top, uint64_t code_start, uint64_t code_end);

#endif

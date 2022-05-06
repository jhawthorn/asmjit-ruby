#include "asmjit.h"

VALUE rb_mAsmjit;

extern "C" void
Init_asmjit(void)
{
  rb_mAsmjit = rb_define_module("AsmJIT");
}

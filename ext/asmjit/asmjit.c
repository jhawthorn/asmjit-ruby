#include "asmjit.h"

VALUE rb_mAsmjit;

void
Init_asmjit(void)
{
  rb_mAsmjit = rb_define_module("Asmjit");
}

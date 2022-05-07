#include <asmjit/asmjit.h>

#include <iostream>
using namespace std;

#include "asmjit.h"

using namespace asmjit;

static VALUE rb_mAsmjit;
static VALUE rb_eAsmJITError;

static JitRuntime jit_runtime;

class RubyErrorHandler : public ErrorHandler {
    public:
        void handleError(Error err, const char* message, BaseEmitter* origin) override {
            rb_raise(rb_eAsmJITError, "AsmJIT error: %s", message);
        }
};
RubyErrorHandler rubyErrorHandler;

struct CodeHolderWrapper {
    CodeHolder *code;
};

void code_holder_free(void *data) {
    CodeHolderWrapper *wrapper = static_cast<CodeHolderWrapper *>(data);
    delete wrapper->code;
    xfree(wrapper);
}

static const rb_data_type_t code_holder_type = {
    .wrap_struct_name = "AsmJIT::CodeHolder",
    .function = {
        .dmark = NULL,
        .dfree = code_holder_free,
        .dsize = NULL,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

VALUE code_holder_alloc(VALUE self) {
    CodeHolderWrapper *wrapper = static_cast<CodeHolderWrapper *>(xmalloc(sizeof(CodeHolderWrapper)));
    wrapper->code = new CodeHolder();
    wrapper->code->setErrorHandler(&rubyErrorHandler);

    return TypedData_Wrap_Struct(self, &code_holder_type, wrapper);
}

VALUE code_holder_initialize(VALUE self) {
    CodeHolderWrapper *wrapper;
    TypedData_Get_Struct(self, CodeHolderWrapper, &code_holder_type, wrapper);

    CodeHolder *code = wrapper->code;

    code->init(jit_runtime.environment());

    //x86::Assembler a(code);

    //a.mov(x86::eax, 1);
    //a.ret();

    //int (*fn)(void);
    //Error err = jit_runtime.add(&fn, code);

    return self;
}

VALUE code_holder_to_ptr(VALUE self) {
    CodeHolderWrapper *wrapper;
    TypedData_Get_Struct(self, CodeHolderWrapper, &code_holder_type, wrapper);

    CodeHolder *code = wrapper->code;

    int (*fn)(void);
    jit_runtime.add(&fn, code);

    return ULL2NUM(uintptr_t(fn));
}

struct x86AssemblerWrapper {
    x86::Assembler *assembler;
    VALUE code_holder;
};

void x86_assembler_free(void *data) {
    x86AssemblerWrapper *wrapper = static_cast<x86AssemblerWrapper *>(data);
    delete wrapper->assembler;
    xfree(wrapper);
}

void x86_assembler_mark(void *data) {
    x86AssemblerWrapper *wrapper = static_cast<x86AssemblerWrapper *>(data);
    rb_gc_mark(wrapper->code_holder);
}

static const rb_data_type_t x86_assembler_type = {
    .wrap_struct_name = "AsmJIT::Assembler",
    .function = {
        .dmark = NULL,
        .dfree = x86_assembler_free,
        .dsize = NULL,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

VALUE x86_assembler_alloc(VALUE self) {
    x86AssemblerWrapper *wrapper = static_cast<x86AssemblerWrapper *>(xmalloc(sizeof(CodeHolderWrapper)));
    wrapper->assembler = NULL;
    wrapper->code_holder = Qnil;

    return TypedData_Wrap_Struct(self, &x86_assembler_type, wrapper);
}

VALUE x86_assembler_initialize(VALUE self, VALUE code_holder) {
    x86AssemblerWrapper *wrapper;
    TypedData_Get_Struct(self, x86AssemblerWrapper, &x86_assembler_type, wrapper);

    CodeHolderWrapper *code_wrapper;
    TypedData_Get_Struct(code_holder, CodeHolderWrapper, &code_holder_type, code_wrapper);

    wrapper->code_holder = code_holder;
    wrapper->assembler = new x86::Assembler(code_wrapper->code);

    return self;
}

Operand parse_operand(VALUE val) {
    if (FIXNUM_P(val)) {
        return Imm(NUM2LL(val));
    } else if (RB_TYPE_P(val, T_SYMBOL)) {
        if (val == ID2SYM(rb_intern("eax"))) {
            return x86::eax;
        } else if (val == ID2SYM(rb_intern("rax"))) {
            return x86::rax;
        }
    }
    rb_raise(rb_eAsmJITError, "bad operand: %" PRIsVALUE, val);
}

VALUE x86_assembler_emit(int argc, VALUE* argv, VALUE self) {
    x86AssemblerWrapper *wrapper;
    TypedData_Get_Struct(self, x86AssemblerWrapper, &x86_assembler_type, wrapper);

    x86::Assembler *assembler = wrapper->assembler;

    if (argc < 1) return Qnil;
    if (argc > 7) return Qnil;

    VALUE insn_name = argv[0];
    InstId inst_id = InstAPI::stringToInstId(assembler->arch(), RSTRING_PTR(insn_name), RSTRING_LEN(insn_name));

    Operand operands[6];
    for (int i = 0; i < argc - 1; i++) {
        operands[i] = parse_operand(argv[i + 1]);
    }

    //assembler->emit(inst_id);
    assembler->emitOpArray(inst_id, &operands[0], argc - 1);

    return self;
}

x86::Inst::Id next_id(x86::Inst::Id id) {
    uint32_t id_int = id;
    return static_cast<x86::Inst::Id>(id_int + 1);
}

extern "C" void
Init_asmjit(void)
{
    //jit_runtime = new JitRuntime();
    rb_mAsmjit = rb_define_module("AsmJIT");

    rb_eAsmJITError = rb_define_class_under(rb_mAsmjit, "Error", rb_eStandardError);

	VALUE cCodeHolder = rb_define_class_under(rb_mAsmjit, "CodeHolder", rb_cObject);
	rb_define_alloc_func(cCodeHolder, code_holder_alloc);
	rb_define_method(cCodeHolder, "initialize", code_holder_initialize, 0);
	rb_define_method(cCodeHolder, "to_ptr", code_holder_to_ptr, 0);

	VALUE rb_mX86 = rb_define_module_under(rb_mAsmjit, "X86");

	VALUE cX86Assembler = rb_define_class_under(rb_mX86, "Assembler", rb_cObject);
	rb_define_alloc_func(cX86Assembler, x86_assembler_alloc);
	rb_define_method(cX86Assembler, "initialize", x86_assembler_initialize, 1);
	rb_define_method(cX86Assembler, "emit", x86_assembler_emit, -1);

    VALUE instructions = rb_ary_new();

    auto instid = x86::Inst::kIdNone;
    while (x86::Inst::isDefinedId(instid)) {
        String str;
        InstAPI::instIdToString(Arch::kX64, instid, str);

        VALUE ruby_str = rb_str_new_cstr(str.data());
        rb_ary_push(instructions, ruby_str);

        instid = next_id(instid);
    }

    rb_define_const(rb_mX86, "INSTRUCTIONS", instructions);
}

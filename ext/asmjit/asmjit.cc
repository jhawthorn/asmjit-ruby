#include <asmjit/asmjit.h>

#include <iostream>
using namespace std;

#include "asmjit.h"

using namespace asmjit;

static VALUE rb_mAsmjit;
static VALUE rb_eAsmJITError;
static VALUE rb_cOperand;
static VALUE cX86Reg;
static VALUE cX86Mem;

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

#define rb_define_method_id_original rb_define_method_id

VALUE code_holder_define_method(VALUE self, VALUE mod, VALUE name, VALUE arity_val) {
    CodeHolderWrapper *wrapper;
    TypedData_Get_Struct(self, CodeHolderWrapper, &code_holder_type, wrapper);

    CodeHolder *code = wrapper->code;

    int arity = FIX2INT(arity_val);
    ID id = rb_sym2id(name);

    VALUE (*fn)(ANYARGS);
    jit_runtime.add(&fn, code);

    // avoid cxxanyargs
#undef rb_define_method_id
    rb_define_method_id(mod, id, fn, arity);

    return name;
}

VALUE code_holder_binary(VALUE self) {
    CodeHolderWrapper *wrapper;
    TypedData_Get_Struct(self, CodeHolderWrapper, &code_holder_type, wrapper);

    CodeHolder *code = wrapper->code;

    CodeBuffer buffer = code->sectionById(0)->buffer();
    return rb_str_new(
            reinterpret_cast<const char *>(buffer.data()),
            buffer.size()
            );
}

static const rb_data_type_t operand_type = {
    .wrap_struct_name = "AsmJIT::Operand",
    .function = {
        .dmark = NULL,
        .dfree = RUBY_DEFAULT_FREE,
        .dsize = NULL,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

struct OperandWrapper {
    Operand opnd;
};

static VALUE build_register(const char *c_name, x86::Reg reg) {
    OperandWrapper *wrapper = static_cast<OperandWrapper *>(xmalloc(sizeof(OperandWrapper)));
    wrapper->opnd = reg;

    VALUE name = ID2SYM(rb_intern(c_name));
    VALUE obj = TypedData_Wrap_Struct(cX86Reg, &operand_type, wrapper);
    rb_iv_set(obj, "@name", name);
    return obj;
}

static VALUE build_label(Label label) {
    OperandWrapper *wrapper = static_cast<OperandWrapper *>(xmalloc(sizeof(OperandWrapper)));
    wrapper->opnd = label;

    VALUE obj = TypedData_Wrap_Struct(cX86Reg, &operand_type, wrapper);
    return obj;
}


static Operand opnd_get(VALUE val) {
    OperandWrapper *wrapper;
    TypedData_Get_Struct(val, OperandWrapper, &operand_type, wrapper);
    return wrapper->opnd;
}

static Label label_get(VALUE val) {
    Operand opnd = opnd_get(val);
    if (!opnd.isLabel()) {
        rb_raise(rb_eTypeError, "operand is not label");
    }
    return opnd.as<Label>();
}

static x86::Reg x86_reg_get(VALUE val) {
    Operand opnd = opnd_get(val);
    if (!opnd.isReg()) {
        rb_raise(rb_eTypeError, "operand is not reg");
    }
    return opnd.as<x86::Reg>();
}

static x86::Mem x86_mem_get(VALUE val) {
    Operand opnd = opnd_get(val);
    if (!opnd.isMem()) {
        rb_raise(rb_eTypeError, "operand is not mem");
    }
    return opnd.as<x86::Mem>();
}

static VALUE x86_ptr(VALUE _self, VALUE regv, VALUE offsetv, VALUE sizev) {
    OperandWrapper *wrapper = static_cast<OperandWrapper *>(xmalloc(sizeof(OperandWrapper)));

    x86::Reg reg = x86_reg_get(regv);
    if (!reg.isGp()) {
        rb_raise(rb_eAsmJITError, "reg must be Gp");
    }
    int32_t offset = NUM2INT(offsetv);
    uint32_t size = NUM2UINT(sizev);
    x86::Mem mem = x86::ptr(reg.as<x86::Gp>(), offset, size);
    wrapper->opnd = mem;

    VALUE obj = TypedData_Wrap_Struct(cX86Mem, &operand_type, wrapper);
    return obj;
}

static VALUE build_registers_hash() {
    VALUE hash = rb_hash_new();

#define REGISTER(name) rb_hash_aset(hash, ID2SYM(rb_intern(#name)), build_register((#name), x86::name))

    REGISTER(eax);
    REGISTER(ebx);
    REGISTER(ecx);
    REGISTER(edx);
    REGISTER(edi);
    REGISTER(esi);

    REGISTER(rax);
    REGISTER(rbx);
    REGISTER(rcx);
    REGISTER(rdx);
    REGISTER(rdi);
    REGISTER(rsi);

#undef REGISTER

    return hash;
}

struct BaseEmitterWrapper {
    BaseEmitter *emitter;
    VALUE code_holder;
};

void x86_assembler_free(void *data) {
    BaseEmitterWrapper *wrapper = static_cast<BaseEmitterWrapper *>(data);
    delete wrapper->emitter;
    xfree(wrapper);
}

void x86_assembler_mark(void *data) {
    BaseEmitterWrapper *wrapper = static_cast<BaseEmitterWrapper *>(data);
    rb_gc_mark(wrapper->code_holder);
}

static const rb_data_type_t base_emitter_type = {
    .wrap_struct_name = "AsmJIT::BaseEmitter",
    .function = {
        .dmark = NULL,
        .dfree = x86_assembler_free,
        .dsize = NULL,
    },
    .data = NULL,
    .flags = RUBY_TYPED_FREE_IMMEDIATELY,
};

VALUE x86_assembler_new(VALUE self, VALUE code_holder) {
    BaseEmitterWrapper *wrapper = static_cast<BaseEmitterWrapper *>(xmalloc(sizeof(CodeHolderWrapper)));

    CodeHolderWrapper *code_wrapper;
    TypedData_Get_Struct(code_holder, CodeHolderWrapper, &code_holder_type, code_wrapper);

    x86::Assembler *assembler = new x86::Assembler(code_wrapper->code);
    assembler->addDiagnosticOptions(DiagnosticOptions::kValidateAssembler);

    wrapper->code_holder = code_holder;
    wrapper->emitter = assembler;

    return TypedData_Wrap_Struct(self, &base_emitter_type, wrapper);
}

Operand parse_operand(VALUE val) {
    if (FIXNUM_P(val)) {
        return Imm(NUM2LL(val));
    } else if (rb_obj_is_kind_of(val, rb_cOperand)) {
        return opnd_get(val);
    }
    rb_raise(rb_eAsmJITError, "bad operand: %" PRIsVALUE, val);
}

VALUE base_emitter_new_label(VALUE self) {
    BaseEmitterWrapper *wrapper;
    TypedData_Get_Struct(self, BaseEmitterWrapper, &base_emitter_type, wrapper);
    BaseEmitter *emitter = wrapper->emitter;

    Label label = emitter->newLabel();
    return build_label(label);
}

VALUE base_emitter_bind(VALUE self, VALUE labelv) {
    BaseEmitterWrapper *wrapper;
    TypedData_Get_Struct(self, BaseEmitterWrapper, &base_emitter_type, wrapper);
    BaseEmitter *emitter = wrapper->emitter;

    Label label = label_get(labelv);

    int err = emitter->bind(label);
    if (err) {
        rb_raise(rb_eAsmJITError, "error binding label");
    }

    return labelv;
}

VALUE base_emitter_emit(int argc, VALUE* argv, VALUE self) {
    BaseEmitterWrapper *wrapper;
    TypedData_Get_Struct(self, BaseEmitterWrapper, &base_emitter_type, wrapper);

    BaseEmitter *emitter = wrapper->emitter;

    if (argc < 1) return Qnil;
    if (argc > 7) return Qnil;

    VALUE insn_name = argv[0];
    Check_Type(insn_name, T_STRING);
    InstId inst_id = InstAPI::stringToInstId(emitter->arch(), RSTRING_PTR(insn_name), RSTRING_LEN(insn_name));

    Operand operands[6];
    for (int i = 0; i < argc - 1; i++) {
        operands[i] = parse_operand(argv[i + 1]);
    }

    emitter->emitOpArray(inst_id, &operands[0], argc - 1);

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
    rb_define_method(cCodeHolder, "def_method", code_holder_define_method, 3);
    rb_define_method(cCodeHolder, "binary", code_holder_binary, 0);

    VALUE rb_mX86 = rb_define_module_under(rb_mAsmjit, "X86");

    VALUE rb_cBaseEmitter = rb_define_class_under(rb_mAsmjit, "BaseEmitter", rb_cObject);
    rb_undef_alloc_func(rb_cBaseEmitter);
    rb_define_method(rb_cBaseEmitter, "_emit", base_emitter_emit, -1);
    rb_define_method(rb_cBaseEmitter, "new_label", base_emitter_new_label, 0);
    rb_define_method(rb_cBaseEmitter, "bind", base_emitter_bind, 1);

    VALUE cX86Assembler = rb_define_class_under(rb_mX86, "Assembler", rb_cBaseEmitter);
    rb_define_singleton_method(cX86Assembler, "new", x86_assembler_new, 1);

    rb_cOperand = rb_define_class_under(rb_mAsmjit, "Operand", rb_cObject);
    rb_undef_alloc_func(rb_cOperand);

    cX86Reg = rb_define_class_under(rb_mX86, "Reg", rb_cOperand);
    rb_define_attr(cX86Reg, "name", 1, 0);

    cX86Mem = rb_define_class_under(rb_mX86, "Mem", rb_cOperand);
    rb_define_singleton_method(cX86Mem, "new", x86_ptr, 3);

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
    rb_define_const(rb_mX86, "REGISTERS", build_registers_hash());
}

# frozen_string_literal: true

require "test_helper"

class TestAsmJIT < AsmJitTest
  include AsmJIT

  def test_that_it_has_a_version_number
    refute_nil ::AsmJIT::VERSION
  end

  def test_x86_instructions_defined
    instructions = AsmJIT::X86::INSTRUCTIONS
    assert_equal 1663, instructions.size
  end

  def test_assembly_binary_output
    code = AsmJIT::CodeHolder.new
    assembler = AsmJIT::X86::Assembler.new(code)
    assembler.mov(:eax, 0x123)
    assembler.mov(:eax, 0x456)

    assert_disasm [
      "mov eax, 0x123",
      "mov eax, 0x456"
    ], code
  end

  def test_assembly_simple_operations
    code = AsmJIT::CodeHolder.new
    asm = AsmJIT::X86::Assembler.new(code)
    asm.mov(:eax, 0x123)
    asm.add(:eax, 0x456)
    asm.mov(:ebx, 1)
    asm.add(:eax, :ebx)

    assert_disasm [
      "mov eax, 0x123",
      "add eax, 0x456",
      "mov ebx, 1",
      "add eax, ebx",
    ], code
  end

  def test_calling_assembled_function
    code = AsmJIT::CodeHolder.new
    assembler = AsmJIT::X86::Assembler.new(code)
    assembler.mov(:eax, 123)
    assembler.ret
    ptr = code.to_ptr

    func = Fiddle::Function.new(
      ptr,
      [],
      Fiddle::TYPE_INT
    )

    ret = func.call
    assert_equal 123, ret
  end

  def test_assembly_memory_operand
    code = CodeHolder.new
    assembler = X86::Assembler.new(code)
    assembler.mov(:rax, X86.qword_ptr(:rcx, 8))

    assert_disasm [
      "mov rax, qword ptr [rcx + 8]",
    ], code
  end

  def test_label
    code = CodeHolder.new
    asm = X86::Assembler.new(code)

    label = asm.new_label

    asm.bind(label)
    asm.sub(:rax, 1)
    asm.jnz(label)

    assert_disasm [
      "sub rax, 1",
      "jne 0"
    ], code
  end

  def test_readme_example
    example = <<~RUBY
      f = AsmJIT.assemble do |a|
        a.mov :eax, 123
        a.ret
      end.to_fiddle

      f.call
      # => 123
    RUBY

    assert_includes File.read("#{__dir__}/../README.md"), example, "README example has been upadted"

    result = eval(example)
    assert_equal 123, result
  end

  def test_defining_ruby_module
    code = AsmJit.assemble do |a|
      a.mov :rax, 5
      a.ret
    end
    mod = code.def_module(0)
    assert_kind_of Module, mod
    obj = Object.new
    obj.extend mod
    assert_equal 2, obj.call
  end

  def test_defining_ruby_class
    code = AsmJit.assemble do |a|
      a.mov :rax, 5
      a.ret
    end
    klass = code.def_class(0)
    assert_kind_of Class, klass
    assert_equal 2, klass.new.call
  end

  def test_errors_on_insufficient_operands
    code = AsmJIT::CodeHolder.new
    assembler = AsmJIT::X86::Assembler.new(code)

    ex = assert_raises AsmJIT::Error do
      assembler.add()
    end
    assert_equal "AsmJIT error: InvalidInstruction: add", ex.message
  end

  def test_errors_on_invalid_operands
    code = AsmJIT::CodeHolder.new
    assembler = AsmJIT::X86::Assembler.new(code)

    ex = assert_raises AsmJIT::Error do
      assembler.add 1, 2
    end
    assert_equal "AsmJIT error: InvalidInstruction: add 1, 2", ex.message
  end
end

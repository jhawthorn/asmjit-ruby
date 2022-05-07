# frozen_string_literal: true

require "test_helper"

class TestAsmJIT < Minitest::Test
  def test_that_it_has_a_version_number
    refute_nil ::AsmJIT::VERSION
  end

  def test_x86_instructions_defined
    instructions = AsmJIT::X86::INSTRUCTIONS
    assert_equal 1663, instructions.size
  end

  def test_it_can_assemble
    code = AsmJIT::CodeHolder.new
    assembler = AsmJIT::X86::Assembler.new(code)
    assembler.mov(:eax, 123)
    assembler.ret
  end

  def test_errors_on_invalid_use
    code = AsmJIT::CodeHolder.new
    assembler = AsmJIT::X86::Assembler.new(code)

    ex = assert_raises AsmJIT::Error do
      assembler.add()
    end
    assert_equal "AsmJIT error: InvalidInstruction: add", ex.message
  end
end

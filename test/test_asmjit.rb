# frozen_string_literal: true

require "test_helper"

class TestAsmJIT < Minitest::Test
  def test_that_it_has_a_version_number
    refute_nil ::AsmJIT::VERSION
  end

  def test_it_does_something_useful
    code = AsmJIT::CodeHolder.new
    assembler = AsmJIT::X86::Assembler.new(code)
    p code
  end
end

# frozen_string_literal: true

$LOAD_PATH.unshift File.expand_path("../lib", __dir__)
require "asmjit"

require "fiddle"
require "hatstone"

require "minitest/autorun"

class AsmJitTest < Minitest::Test
  def x86_disasm(input, address: 0x0)
    hs = Hatstone.new(Hatstone::ARCH_X86, Hatstone::MODE_64)
    hs.disasm(input, address).map do |insn|
      "#{insn.mnemonic} #{insn.op_str}"
    end
  end

  def assert_disasm(expected, code)
    binary = code.binary
    disasm = x86_disasm(binary)
    assert_equal expected, disasm
  end
end

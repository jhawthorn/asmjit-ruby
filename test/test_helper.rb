# frozen_string_literal: true

$LOAD_PATH.unshift File.expand_path("../lib", __dir__)
require "asmjit"

require "fiddle"

require "minitest/autorun"

class AsmJitTest < Minitest::Test
  def assert_disasm(expected, code)
    refute_nil code.logger

    assert_equal expected, code.logger.lines.map(&:chomp)
  end
end

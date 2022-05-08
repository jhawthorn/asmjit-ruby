# frozen_string_literal: true

require_relative "asmjit/version"
require_relative "asmjit/asmjit"

module AsmJIT
  def self.assemble
    code = CodeHolder.new
    yield code.assembler
    code
  end

  class CodeHolder
    def assembler
      X86::Assembler.new(self)
    end

    def to_fiddle(inputs: nil, output: nil)
      ptr = to_ptr

      Fiddle::Function.new(
        ptr,
        inputs || [],
        output || Fiddle::TYPE_INT
      )
    end
  end

  module X86
    class Assembler
      INSTRUCTIONS.each do |name|
        define_method(name) do |*args|
          emit(name, *args)
        end
      end
    end
  end
end

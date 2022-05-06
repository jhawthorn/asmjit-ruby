# frozen_string_literal: true

require_relative "asmjit/version"
require_relative "asmjit/asmjit"

module AsmJIT
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

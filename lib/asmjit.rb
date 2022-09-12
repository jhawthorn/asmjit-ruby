# frozen_string_literal: true

require_relative "asmjit/version"
require_relative "asmjit/asmjit"

AsmJit = AsmJIT
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

    def def_module(arity, method_name: :call)
      mod = Module.new
      self.def_method(mod, method_name, arity)
      mod
    end

    def def_class(arity, method_name: :call)
      mod = Class.new
      self.def_method(mod, method_name, arity)
      mod
    end
  end

  module X86
    module Helpers
      extend self

      def parse_operand(arg)
        if Symbol === arg && reg = REGISTERS[arg]
          reg
        else
          arg
        end
      end
    end

    class << self
      def ptr(base, offset, size)
        X86::Mem.new(Helpers.parse_operand(base), offset, size)
      end

      def qword_ptr(base, offset)
        ptr(base, offset, 8)
      end
    end

    class Assembler
      def emit(*args)
        _emit(*(args.map { |arg| Helpers.parse_operand(arg) }))
      end

      INSTRUCTIONS.each do |name|
        define_method(name) do |*args|
          emit(name, *args)
        end
      end
    end
    class Reg
      def inspect
        "#<#{self.class} #{name}>"
      end
    end
  end
end

# frozen_string_literal: true

require "mkmf"

asmjit_dir = File.expand_path("asmjit/src/", __dir__)

$INCFLAGS << " -I#{asmjit_dir} "

$CXXFLAGS << " -DASMJIT_EMBED "

$srcs = []
$srcs.concat Dir[File.join(__dir__, "*.cc")]
$srcs.concat Dir[File.join(asmjit_dir, "**/*.cpp")]

$objs = $srcs.map{|x| x.gsub(/\.(cc|cpp)\z/, ".o") }

create_makefile("asmjit/asmjit")

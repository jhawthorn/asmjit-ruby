# frozen_string_literal: true

require "mkmf"

Dir.chdir __dir__ do
  $srcs = [
    Dir["*.cc"],
    Dir["asmjit/src/**/*.cpp"]
  ].flatten.map { |f| File.basename f }

  $VPATH.concat Dir["asmjit/src/**/"].map { |x| "$(srcdir)/#{x}" }
end

append_cppflags("-I$(srcdir)/asmjit/src")
append_cppflags("-DASMJIT_EMBED")

create_makefile("asmjit/asmjit")

# frozen_string_literal: true

require "bundler/gem_tasks"
require "rake/testtask"

Rake::TestTask.new(:test) do |t|
  t.libs << "test" << "lib"
  t.test_files = FileList["test/**/test_*.rb"]
  t.verbose = true
  t.warning = true
end
task test: :compile

require "rake/extensiontask"

task build: :compile

Rake::ExtensionTask.new("asmjit") do |ext|
  ext.lib_dir = "lib/asmjit"
end

task default: %i[test]

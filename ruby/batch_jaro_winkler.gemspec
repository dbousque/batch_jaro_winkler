

require File.expand_path('../lib/batch_jaro_winkler/version', __FILE__)

Gem::Specification.new do |s|
  s.name        = 'batch_jaro_winkler'
  s.version     = BatchJaroWinkler::VERSION
  s.license     = 'MIT'
  s.summary     = 'Fast batch jaro winkler distance implementation in C99.'
  s.description = 'This project gets its performance from the pre-calculation of an optimized model in advance of the actual runtime calculations. Supports any encoding.'
  s.authors     = ['Dominik Bousquet']
  s.email       = 'bousquet.dominik@gmail.com'
  s.homepage    = 'https://github.com/dbousque/batch_jaro_winkler'
  s.metadata    = { 'source_code_uri' => 'https://github.com/dbousque' }
  s.extensions  = ['ext/batch_jaro_winkler/extconf.rb']
  s.files       = [
    'lib/batch_jaro_winkler.rb',
    'lib/batch_jaro_winkler/version.rb'
  ] + Dir['ext/batch_jaro_winkler/*', 'ext/batch_jaro_winkler/ext/*']
  s.require_paths = ['lib']
  s.required_ruby_version = ">= 2.1.0"

  s.add_runtime_dependency 'ffi', '~> 1.12', '>= 1.12.2'
end
%% Generate C code from dtmf_decode.m (requires MATLAB Coder)
% MATLAB Coder produces dtmf_decode.c/.h and supporting code.
clear; clc;
assert(exist('codegen','file') ~= 0, 'MATLAB Coder is required.');
config = coder.config('lib');
config.TargetLang = 'C';
codegen -config config dtmf_decode -args {zeros(200,1,'uint16')} -report
fprintf('Generated C files can be found under codegen/lib/dtmf_decode/\n');


xn = zeros(128, 1);
k = 5;



function output = goertzel(xn, k)
    output = goerzel(xn, k+1);
end

%'Take this code and run code-gen in matlab command window'
%cfg = coder.config('lib');   % generate C code as a library
%codegen -config cfg goertzel -args {zeros(128,1), 5}


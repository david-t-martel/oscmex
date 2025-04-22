clear
clc
close all hidden

% Define the source C++ file
mexSource = fullfile('C:\codedev\auricleinc\oscmex\src\');
local_dir = fullfile(mexSource,'oscmix');
sourceFile = fullfile(local_dir,'osc_rme.cpp');

% Define the include directories for liblo and oscmix
liblo_dir = fullfile(mexSource,'\liblo');
%liblo_dir_head = fullfile(liblo_dir,'lo');
includeDirs = {...
    local_dir,...
    fullfile(local_dir,'lo')...
    };
%fullfile(mexSource,'\oscmix')   ,...
%    liblo_dir, ...% Replace with the actual path to your liblo include directory
%    liblo_dir_head,% Replace with the actual path to your oscmix directory
%};

% Define the libraries to link against
libraries = {
    '-llo' % Link against the liblo library
};

% Construct the mex command with include directories and libraries
outDir = fullfile(mexSource,'oscmix\matlab');

mexCommand = sprintf('mex -v -R2018a "%s" -I"%s" %s -outdir "%s"', ...
    sourceFile, ...
    strjoin(includeDirs, '" -I"'), strjoin(libraries),...
    outDir);

% Execute the mex command
eval(mexCommand);

% Display a success message
disp('MEX function compiled successfully!');
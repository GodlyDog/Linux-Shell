To compile the shell, the supported make commands are:
make 33noprompt (Generates shell with no prompt flag, which removes the new line prompt and some message output)
make 33sh (Standard shell with normal messaging)
make clean
make clean all

To run a command in the background, enter the command with & as the final character.
Currently supported builtins:
cd, fg, bg, exit, rm
Supports program execution in both foreground and background by passing the filepath as /.../<filepath> followed by any args
Other standard builtins can be executed from /bin
(e.g. /bin/ls)
Supports file redirection with <, >, >>

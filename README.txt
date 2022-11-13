The shell project has a main function, a parse function, and an exec_handler function. The parse function
generates the argument vector from passed in strings, but does not handle file path trimming or file redirects.

The main function handles the main functionality and error checking of the program, and also checks for file redirects.

The exec_handler function sets the file redirects according to the flags set in main, and then executes the entered
command if it is a valid command. The command may be entered as a background command by passing & as the final argument,
or it will run in the foreground by default.

The grim function handles reaping and any changes to the status of processes running in the background.

The fg function handles resuming a stopped process in the foreground.

To compile the shell, the supported make commands are:
make 33noprompt
make 33sh
make clean
make clean all

The project was done solo, and there are no known bugs.
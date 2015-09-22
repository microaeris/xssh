Y88b   d88P  .d8888b.   .d8888b.  888    888 
 Y88b d88P  d88P  Y88b d88P  Y88b 888    888 
  Y88o88P   Y88b.      Y88b.      888    888 
   Y888P     "Y888b.    "Y888b.   8888888888 
   d888b        "Y88b.     "Y88b. 888    888 
  d88888b         "888       "888 888    888 
 d88P Y88b  Y88b  d88P Y88b  d88P 888    888 
d88P   Y88b  "Y8888P"   "Y8888P"  888    888 

ABOUT

    Assignment for CSE 422 - Operating Systems from my spring semester of 
    Junior year. This is a single file pseudo shell to run in your shell! 

USAGE

    Lab instructions are included in the file:
    CSE422 Spring 2015 - Lab1 Instructions.pdf

    XSSH takes in commands of the format:
    xssh [-x] [-d <level>] [-f file [arg] ... ]

    What other fun things can this shell do?
    - Internal commands (show, set, unset, export, etc.)
    - External commands (fork/execs other programs)
    - Supports search paths (absolute, relative, from PATH)
    - Background commands (use "&" to run a process in the bg)
    - Local and global variables
    - Variable substitution ("$" to denote variables)
    - Special variable substitution ($$, $?, $!)
    - Stdin/Stdout redirection
    - Handles terminal-generated signals
    - Has a fancy command line prompt
    - Ignores your #comments
    - Can run the pseudo shell within itself within itself B-)

AUTHOR

    Alice Wang, Feb 5, 2015

    For more things at the intersection of CS and cute: 
    https://github.com/Ahris/

INSTALL

    Use your favorite C compiler! I personally used gcc:
    gcc -o xssh xssh.c -std=c99
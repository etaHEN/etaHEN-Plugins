clang --target=x86_64-freebsd-pc-elf -fPIC -fPIE -fomit-frame-pointer -Wall -Werror -gfull -gdwarf-2 -O3 -march=znver2 -mavx2 -c BO6_Shellcode.c -o BO6_Shellcode.o

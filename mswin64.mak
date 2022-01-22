# makefile
# Nash!Com / Daniel Nashed
# Windows 64-bit version using 
# Microsoft Visual Studio 2017

PROGRAM=nshwsl
NODEBUG=1

all: $(PROGRAM).exe

# Link command
$(PROGRAM).exe: $(PROGRAM).obj
	link /SUBSYSTEM:CONSOLE $(PROGRAM).obj msvcrt.lib user32.lib -out:$@

# Compile command
$(PROGRAM).obj: $(PROGRAM).cpp
	cl -nologo -c -D_MT -MT /Zi /Ot /O2 /Ob2 /Oy- -Gd /Gy /GF /Gs4096 /GS- /favor:INTEL64 /EHsc /Zc:wchar_t- -Zl -W1 -D_AMD64_  $(PROGRAM).cpp

clean:
	del *.obj *.exe


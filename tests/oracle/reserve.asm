; Reserves the reference XBE's address range inside the oracle executables (analysis tooling, Windows x86 only).
; A real code segment named .text, in the first object linked, sits ahead of all other code, so with /BASE:0x10000 this starts at 0x00011000,
; exactly where the XBE's .text starts. No original bytes here: tests/oracle/oracle_image.h copies them in at run time.
.386
.model flat
PUBLIC _g_low
reserved SEGMENT PARA PUBLIC FLAT ALIAS(".text") EXECUTE READ 'CODE'
_g_low DB 3EF000h DUP(0)
reserved ENDS
END

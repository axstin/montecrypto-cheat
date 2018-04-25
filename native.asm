.code

; Because you can't use inline asm when compiling for x64 with MSVC -___-

__get_rsp PROC
lea rax, [rsp + 8]
ret
__get_rsp ENDP

END
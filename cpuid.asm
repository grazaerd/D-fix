.code
IsAMD PROC
    xor eax, eax
    cpuid                                    
    cmp ecx, 444d4163h         
    setz al      
    ret
IsAMD ENDP
end
.code
cpuidfn PROC
    xor eax, eax
    cpuid                                    
    cmp ecx, 444d4163h         
    setz al      
    ret
cpuidfn ENDP
end
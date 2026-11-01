.intel_syntax noprefix
.text
.globl _controller_test_invoke
/* cdecl harness checks the stack after a stdcall COM method returns. */
_controller_test_invoke:
    push ebp
    mov ebp, esp
    push ebx
    push esi
    push edi
    mov ebx, esp
    mov esi, dword ptr [ebp+8]
    mov ecx, dword ptr [ebp+16]
    mov edx, dword ptr [ebp+20]
1:
    test ecx, ecx
    jz 2f
    dec ecx
    push dword ptr [edx+ecx*4]
    jmp 1b
2:
    push dword ptr [ebp+12]
    call esi
    mov edx, dword ptr [ebp+24]
    mov dword ptr [edx], eax
    xor eax, eax
    cmp esp, ebx
    sete al
    mov esp, ebx
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret

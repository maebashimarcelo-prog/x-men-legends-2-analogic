.intel_syntax noprefix
.text
.globl _analog_direction_getter
.extern _analog_direction_value
.extern _g_original_direction_getter
/* thiscall getter; float returned in ST(0); original ret 4. */
_analog_direction_getter:
    push ecx
    sub esp, 4
    mov eax, DWORD PTR [esp+8]
    mov edx, DWORD PTR [esp+12]
    lea ecx, [esp]
    push ecx
    push eax
    push edx
    call _analog_direction_value
    add esp, 12
    cmp eax, 2
    je .Lmenu_invert
    test eax, eax
    jz .Lfallback
    fld DWORD PTR [esp]
    add esp, 4
    pop ecx
    ret 4
.Lfallback:
    add esp, 4
    pop ecx
    jmp DWORD PTR [_g_original_direction_getter]

/* Call original getter, then invert its x87 floating-point return value.
   Original thiscall getter consumes its single stack argument (ret 4),
   so this trampoline returns with plain ret. */
.Lmenu_invert:
    add esp, 4
    pop ecx
    call DWORD PTR [_g_original_direction_getter]
    fchs
    ret

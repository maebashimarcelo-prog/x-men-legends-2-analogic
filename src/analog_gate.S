.intel_syntax noprefix
.text
.globl _analog_gate_stub
.extern _g_gate_resume
.extern _g_deadzone_sq
.extern _g_zero_threshold
.extern _g_retail_threshold
.extern _g_wrapper_cell
.extern _g_controller_mask
.extern _g_pad_offset
.extern _g_gate_calls
.extern _g_gate_movement
.extern _g_gate_selected_pad
.extern _g_gate_radial

/* Replaces only the validated FCOMP. ST(0) holds abs(action). The original
   code resumes immediately after the six replaced bytes; [esp+0x10] is the
   destination slot. Movement is slots 0/1 only. */
_analog_gate_stub:
    pushfd
    inc dword ptr [_g_gate_calls]
    push eax
    push ecx
    push edx
    mov ecx, dword ptr [esp+0x20]
    cmp ecx, 1
    ja .Loriginal
    inc dword ptr [_g_gate_movement]

    mov eax, dword ptr [_g_wrapper_cell]
    mov eax, dword ptr [eax]
    test eax, eax
    jz .Loriginal
    mov edx, dword ptr [eax+0x129cc]
    test edx, dword ptr [_g_controller_mask]
    jz .Loriginal
    inc dword ptr [_g_gate_selected_pad]
    mov ecx, dword ptr [_g_pad_offset]
    mov ecx, dword ptr [eax+ecx]
    imul ecx, ecx
    mov edx, dword ptr [_g_pad_offset]
    mov edx, dword ptr [eax+edx+4]
    imul edx, edx
    add ecx, edx
    cmp ecx, dword ptr [_g_deadzone_sq]
    jbe .Loriginal
    inc dword ptr [_g_gate_radial]

    pop edx
    pop ecx
    pop eax
    popfd
    fcomp dword ptr [_g_zero_threshold]
    jmp dword ptr [_g_gate_resume]

.Loriginal:
    pop edx
    pop ecx
    pop eax
    popfd
    fcomp dword ptr [_g_retail_threshold]
    jmp dword ptr [_g_gate_resume]

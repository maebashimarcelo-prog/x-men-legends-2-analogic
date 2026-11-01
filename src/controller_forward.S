.intel_syntax noprefix
.text
/* Every untouched COM method forwards with the original object as 'this'.
   No original DirectInput vtable or object memory is changed. */
.macro forward index
_controller_forward_\index:
    mov eax, dword ptr [esp+4]
    mov eax, dword ptr [eax+4]
    mov dword ptr [esp+4], eax
    mov eax, dword ptr [eax]
    jmp dword ptr [eax+4*\index]
.endm
.irp n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
forward \n
.endr
.data
.globl _controller_forward_table
_controller_forward_table:
.irp n,0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
.long _controller_forward_\n
.endr

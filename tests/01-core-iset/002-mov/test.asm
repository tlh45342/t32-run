; T32 MOV register-to-register test.
;
; MOV must copy r0 into r1 without changing flags.

movi r0, 42
mov r1, r0
halt

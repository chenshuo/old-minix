.define ___sigreturn
.extern __sigreturn
___sigreturn: 
add sp, #4
j __sigreturn

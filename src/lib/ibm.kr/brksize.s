.define __brksize
.data
.extern endbss, __brksize
__brksize: .word  endbss

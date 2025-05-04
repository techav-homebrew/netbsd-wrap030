#define DEBUG_BOOTSTRAP
    
    .equ    acia1Com,   0x80080000
    .equ    acia1Dat,   acia1Com+4

|; macro to print immediate string
    .macro debugPrintStrI str
    movem.l %a0/%d0,%sp@-               |; save registers
    lea     %pc@(L\@Str),%a0            |; get pointer to string
L\@nxtChr:
    move.b  %a0@+,%d0                   |; get next byte in string
    beq     L\@mexit                    |; end if null
L\@lp:
    btst    #1,acia1Com                 |; check ACIA txrdy bit
    beq     L\@lp                       |; loop until ready
    move.b  %d0,acia1Dat                |; print byte
    bra     L\@nxtChr                   |; loop until done
L\@Str:
    .ascii  "\str\0"
    .even
L\@mexit:
    movem.l %sp@+,%a0/%d0               |; restore working registers
    .endm
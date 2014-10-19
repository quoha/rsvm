rsvm
====

Really Simple Virtual Machine

RSVM is based on the description of BCPL's INTCODE interpreter from these two documents:

    http://www.cl.cam.ac.uk/~mr10/bcplman.pdf
    http://www.gtoal.com/languages/bcpl/amiga/bcpl/booting.txt

RSVM contains only 8 instructions and is easy to extend via the exop instruction. It currently supports
an 8-bit word (a single byte). I plan on adding in 16-, 32-, and 64-bit words.

No matter what the word size is, the instructions are:

```
   4 bits for the function/operation
   1 bit each for the D, P, G, and I modifiers
   remaining bits are used as data
```

The 8 bits that we use are always in the word's "high" byte so that we don't have to shift things around
to use the data.

Note that there could be 16 instructions because we use 4 bits for the function.

Instruction layout with word size of 8 bits.

```
   bit   7     is  the D bit, D loaded from following word
   bit   6     is  the P bit, P to be added to D
   bit   5     is  the G bit, G to be added to D
   bit   4     is  the I bit, D loaded indirectly from D
   bits  3...0 are the function bits
```

Note that only 256 words are addressable by the program counter.

Instruction layout with word size of 16 bits.

```
   bit  15     is  the D bit, D loaded from following word
   bit  14     is  the P bit, P to be added to D
   bit  13     is  the G bit, G to be added to D
   bit  13     is  the I bit, D loaded indirectly from D
   bits 11...8 are the function bits
   bits  7...0 are used in various ways
               8 bits, 1 byte, range -128..+127
```

Note that with a 16 bit address, core is limited to 65k cells.

Instruction layout with word size of 32 bits.

```
   bit  31     is  the D bit, D loaded from following word
   bit  30     is  the P bit, P to be added to D
   bit  29     is  the G bit, G to be added to D
   bit  28     is  the I bit, D loaded indirectly from D
   bits 27..24 are the function bits
   bits 23...0 are used in various ways
               24 bits, 3 bytes, range -8,388,608..+8,388,607
```

Instruction layout with word size of 64 bits.

```
   bit  63     is  the D bit, D loaded from following word
   bit  62     is  the P bit, P to be added to D
   bit  61     is  the G bit, G to be added to D
   bit  60     is  the I bit, D loaded indirectly from D
   bits 59..56 are the function bits
   bits 55...0 are used in various ways
               56 bits, 7 bytes, range is wowsers
```

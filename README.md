# mycc

A toy c compiler for learning purpose, support some C99 features and generate code for x86_64.

# References

## C

* [C99 Gramer](https://slebok.github.io/zoo/c/c99/iso-9899-tc3/extracted/index.html#struct-declaration)
* [C standards](https://stackoverflow.com/a/83763)
* C99 [html](http://port70.net/%7Ensz/c/c99/n1256.html) [pdf](https://www.open-std.org/jtc1/sc22/wg14/www/docs/n1256.pdf)
* [C operator precedence](https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B#Operator_precedence)
* [Understand integer conversion rules(Integer Promotions, Integer Conversion Rank, Usual Arithmetic Conversions)](https://wiki.sei.cmu.edu/confluence/display/c/INT02-C.+Understand+integer+conversion+rules), [C99(6.3)](http://port70.net/~nsz/c/c99/n1256.html#6.3)
* [Implicity prototype C99 vs C90](https://stackoverflow.com/a/437763)
* [alignment](http://www.catb.org/esr/structure-packing/#_alignment_requirements)

## x86

* [Guide to x86-64](https://web.stanford.edu/class/archive/cs/cs107/cs107.1222/guide/x86-64.html)
* [x86 and amd64 instruction reference](https://www.felixcloutier.com/x86/index.html)
* [gas manual](https://sourceware.org/binutils/docs-2.38/as.html)
* [x86_64-abi (3.2 Function Calling Sequence)](https://refspecs.linuxbase.org/elf/x86_64-abi-0.21.pdf)
* [call printf in x86_64 using gas](https://stackoverflow.com/questions/38335212/calling-printf-in-x86-64-using-gnu-assembler#answer-38335743)
* [RPI relative addressing](https://stackoverflow.com/questions/44967075/why-does-this-movss-instruction-use-rip-relative-addressing)

## Concept

* [Computer Systems A Programmers Perspective (3rd)](https://github.com/Sorosliu1029/CSAPP-Labs/blob/master/Computer%20Systems%20A%20Programmers%20Perspective%20(3rd).pdf)
* [Recursive descent parser](https://en.wikipedia.org/wiki/Recursive_descent_parser)
* [LL parser](https://en.wikipedia.org/wiki/LL_parser)
* Scope [wiki](https://en.wikipedia.org/wiki/Scope_(computer_science)) [C](http://port70.net/~nsz/c/c99/n1256.html#6.2.1)


## Some Implements
* [chibicc](https://github.com/rui314/chibicc)
* [lcc](https://github.com/drh/lcc) ( [book](https://cpentalk.com/drive/index.php?download=true&p=Compiler+Design+Books%2FBooks%28+CPENTalk.com+%29&dl=A+Retargetable+C+Compiler+Design+and+Implementation+%28+CPENTalk.com+%29.pdf) )
* [acwj](https://github.com/DoctorWkt/acwj)

## Others

* [How to disable GNU C extensions?](https://stackoverflow.com/a/38940030)
* [Makefile header dependencies](https://stackoverflow.com/a/30142139)


# TODO

- [ ] [Types](http://port70.net/~nsz/c/c99/n1256.html#6.2.5)
    - [ ] integrial types
        - [x] char
        - [x] shrot
        - [x] int
        - [x] long
        - [ ] long long
    - [ ] floaing types
    - [x] pointer types
    - [x] enumeration types
    - [x] function types
        - [ ] returing struct/union or taking sruct/union as paramenter
    - [x] array types
        - [ ] variable length array type
    - [x] struct types
    - [x] union types
    - [ ] type alignment
- [ ] [Conversions](http://port70.net/~nsz/c/c99/n1256.html#6.3)
    - [ ] floating types
- [ ] [Lexical elements](http://port70.net/~nsz/c/c99/n1256.html#6.4)
    - [ ] [constant](http://port70.net/~nsz/c/c99/n1256.html#6.4.4)
        - [x] [integral constant](http://port70.net/~nsz/c/c99/n1256.html#6.4.4)
            - [x] decimal
            - [ ] octal / hex
            - [ ] suffix
        - [ ] floating constant
        - [ ] character constant
        - [x] string literials
            - [x] simple escape
            - [ ] octal/hex escape
    - [ ] comments
- [ ] [Statements](http://port70.net/~nsz/c/c99/n1256.html#6.8)
    - [ ] switch statement
    - [ ] goto statement
- [ ] [Expressions](http://port70.net/~nsz/c/c99/n1256.html#6.5)
    - [x] operand constrains: null pointer? type qualifer? type compatible?
    - [x] sizeof type-name
    - [x] struct/union related operator
    - [ ] cast operator
- [ ] [Declarations](http://port70.net/~nsz/c/c99/n1256.html#6.7)
    - [ ] [typedef](http://port70.net/~nsz/c/c99/n1256.html#6.7.7)
    - [ ] [initialization](http://port70.net/~nsz/c/c99/n1256.html#6.7.8)
    - [ ] declaration of function without name
    - [ ] [struct/union with bit-field member](http://port70.net/~nsz/c/c99/n1256.html#6.7.2.1p9)
    - [ ] [struct/union with flexible array member](http://port70.net/~nsz/c/c99/n1256.html#6.7.2.1p16)
- [ ] [Preprocessing](http://port70.net/~nsz/c/c99/n1256.html#6.10)
- [ ] Others
    - [x] remove print statement by calling library function "printf"
    - [x] for test, compile the source code with gcc, use the output of gcc as target output
    - [ ] for debug: add corrsponding source line in output file 
    - [ ] add source location in error message
    - [ ] read srouce code from file
# References

## C

* [The syntax of C in Backus-Naur Form](https://cs.wmich.edu/~gupta/teaching/cs4850/sumII06/The%20syntax%20of%20C%20in%20Backus-Naur%20form.htm)
* [C standards](https://stackoverflow.com/a/83763) , [C99](http://port70.net/%7Ensz/c/c99/n1256.html)
* [C operator precedence](https://en.wikipedia.org/wiki/Operators_in_C_and_C%2B%2B#Operator_precedence)
* [Understand integer conversion rules(Integer Promotions, Integer Conversion Rank, Usual Arithmetic Conversions)](https://wiki.sei.cmu.edu/confluence/display/c/INT02-C.+Understand+integer+conversion+rules), [C99(6.3)](http://port70.net/~nsz/c/c99/n1256.html#6.3)
* [Implicity prototype C99 vs C90](https://stackoverflow.com/a/437763)

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
    - [ ] enumerated types
    - [ ] function types
    - [x] array types [array declarator](http://port70.net/~nsz/c/c99/n1256.html#6.7.5.2)
    - [ ] struct types
    - [ ] union types
- [ ] [Conversions](http://port70.net/~nsz/c/c99/n1256.html#6.3)
    - [x] integral
    - [ ] floating
    - [x] type conversions: arguments to paramenter
- [ ] [Lexical elements](http://port70.net/~nsz/c/c99/n1256.html#6.4)
    - [ ] [constant](http://port70.net/~nsz/c/c99/n1256.html#6.4.4)
        - [=] integral constant
        - [ ] floating constant
        - [ ] character constant
    - [x] string literials
- [ ] [Expressions](http://port70.net/~nsz/c/c99/n1256.html#6.5)
- [ ] [declarations](http://port70.net/~nsz/c/c99/n1256.html#6.7)
    - [x] init list
- [ ] Others
    - [ ] remove print statement by calling library function "printf"
    - [ ] for test, compile the source code with gcc, use the output of gcc as target output
    - [ ] for debug: add corrsponding source line in output file 
    - [ ] add source location in error message
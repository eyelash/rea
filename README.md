Rea
===

Just a toy compiler

```
func checkPrime(n) {
    var i = 2
    while (i * i <= n) {
        if (n % i == 0) {
            print(i)
            print(n / i)
        }
        var i = i + 1
    }
}

func main() {
    checkPrime(6011003)
}
```

how to
------

```sh
# download the source
$ git clone https://github.com/eyelash/rea.git
$ cd rea

# compile the compiler
$ clang++ -o rea -std=c++11 main.cpp parser.cpp writer.cpp

# compile the standard library
$ clang -c stdlib.c

# compile some code
$ ./rea examples/checkPrime.rea > checkPrime.ll && clang -o checkPrime checkPrime.ll stdlib.o

# and finally execute it
$ ./checkPrime
```

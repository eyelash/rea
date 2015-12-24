Rea
===

Just a toy compiler

```
func isPrime(n: Int): Bool {
    var i = 2
    while i * i <= n {
        if n % i == 0 {
            return false
        }
        i = i + 1
    }
    return true
}

func main() {
    // print the first 10 primes greater than one million
    var n = 0
    var i = 1000000
    while n < 10 {
        if i.isPrime() {
            i.print()
            n = n + 1
        }
        i = i + 1
    }
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
$ ./rea examples/primes.rea > primes.ll && clang -o primes primes.ll stdlib.o

# and finally execute it
$ ./primes
```

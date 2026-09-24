The `examples/` folder contains some small Makefile + C projects that
make use of AVRtest syscalls.

## The maximal relative Error as a Graph

The `plot.svg` and `plot.png` Makefile targets in `examples/plot-delta-float`
can be used to generate a graphic representation of the relative errors of
functions from `math.h`.
It requires [Gnuplot](http://www.gnuplot.info/). The example

![expf relative error over [-1,10] in ULPs](images/expf-m4-10-ulp.svg)

has been generated with
``` none
$ make plot.svg NX=50000 LO='-4' HI='10' DIM=600,400 OUT=ulp FUNC=expf
```
It shows the relative error of `expf` over the interval [-4, 10] in
[ULPs](https://en.wikipedia.org/wiki/Unit_in_the_last_place).

The recognized parameters can be displayed by running `make help`.
Supported functions are univariate float&rarr;float and
long double&rarr;long double functions from math.h.

## The maximal relative Error

The `delta` Makefile target in `examples/delta-float` can be used to work out
the maximal relative error of a univariate floating point function from math.h.

It calculates the relative error for every value x in the specified
interval with a given ULP stride.  For example, a stride of 1 will
calculate the error for every x value in the interval.  This can be
quite time consuming since the function has to be evaluated at
up to 2<sup>32</sup> places.  To that end, the Makefile allows to
run the calculations in parallel.  Here is an example with NUM=2 processes:

```none
$ nice -10 make delta NUM=2 LO=0.9 HI=1 STEP=1 FUNC=asinf
```
The output is something like:
```none
...
NUM=2: [9.000000e-01, 1.000000e+00] += 0x1
1677724 values = 0.84M/run = 0.39 min expected execution time
== 0/2: 0x3f667136: 9.001650e-01 -> 2.128456e-07
== 1/2: 0x3f66863b: 9.004857e-01 -> 2.127056e-07
eval: 0/2: 0x3f667136 = 9.001650e-01 -> 2.128456e-07  log10: -6.671935  log2: -22.163689
```
So the maximal relative error is around 2.128&middot;10<sup>-7</sup>
and is realized for a float value with hex representation
0x3f667136 &asymp; 9.001650&middot;10<sup>-1</sup>.

## Recognized Makefile Variables

In order to determine the recognized Makefile variables that can be
used to adjust the example, run
```none
$ make help
```
in the respective folder.

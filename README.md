# img2vec

Transforming bitmaps into vector graphics.

## How to build

```
$ make
```

## How to use

```
$ ./img2vec
Usage: ./img2vec [options] file

Options:
-h                 Print this message
-o <output name>   Output file name [default: img2vec.eps]
-svg               Output file type [default: eps]
-c <num>           Reduce color [default: 32]
-b <scale>         Blur image

$ ./img2vec girl-1118419_1280.jpg -c 2 -o girl-1118419.eps
$ ./img2vec publicdomainq-0041064ikt.jpg -c 8 -a -b 12 -o publicdomainq-0041064ikt.eps
$ ./img2vec publicdomainq-0017653mro.jpg -c 5 -a -o publicdomainq-0017653mro.eps
$ ./img2vec hairdress-4912246.jpg -o hairdress-4912246.svg -svg -a

$ ./img2vec girl-4716186_1920.jpg -o girl-4716186.svg -svg -a -s 0.4 -c 48
$ ./img2vec girl-4716186_1920.jpg -o girl-4716186_1920.svg -svg -a -s 0.6 -c 48 -x

$ ./img2vec sparkler-677774_1920.jpg -o sparkler-677774.svg -svg -s 0.4 -c 64
$ ./img2vec 2435687439_17e1f58a9c_o.jpg -o 2435687439_17e1f58a9c_o.svg -svg

$ ./img2vec 1098751.jpg -o 1098751.eps -c 48 -a -cx -b 14
$ ./img2vec painting-4820485_1920.jpg -o painting-4820485.svg -svg -s 0.3 -c 18 -cx

$ ./img2vec night-4926430_1920.jpg -c 16 -o night-4926430.eps
$ ./img2vec night-4926430_1920.jpg -o night-4926430.svg -svg -s 0.3
```

## Example

Original image (https://pixabay.com/ja/illustrations/%E5%A5%B3%E3%81%AE%E5%AD%90-%E7%8C%AB-%E8%8A%B1-%E3%81%8A%E3%81%A8%E3%81%8E%E8%A9%B1-1118419/
)
![Original](girl-1118419_1280.jpg)

img2vec output
![Output](girl-1118419.svg)

Original image
![Original](publicdomainq-0041064ikt.jpg)

img2vec output
![Output](publicdomainq-0041064ikt.svg)

Original image
![Original](publicdomainq-0017653mro.jpg)

img2vec output
![Output](publicdomainq-0017653mro.svg)

Original image
![Original](night-4926430_1920.jpg)

img2vec output
![Output](night-4926430.svg)

Original image
![Original](girl-4716186_1920.jpg)

img2vec output
![Output](girl-4716186.svg)

Original image
![Original](sparkler-677774_1920.jpg)

img2vec output
![Output](sparkler-677774.svg)

Original image
![Original](2435687439_17e1f58a9c_o.jpg)

img2vec output
![Output](2435687439_17e1f58a9c_o.svg)


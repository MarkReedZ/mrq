# mrq
The fastest in memory message queue

To build and run the benchmark first build libae. We're using the redis event loop as it is 20% faster than libuv in my tests.

```
  cd ae
  gcc -c -Wno-parentheses -Wno-switch-enum -Wno-unused-value ae.c -o ae.o
  gcc -c -Wno-parentheses -Wno-switch-enum -Wno-unused-value anet.c -o anet.o
  ar -rc libae.a ae.o anet.o
```

Then

```
  sh bldae
  ./mrq
  python3 tst.py
```

I get 100m requests per second vs nats at 2.7m on my machine


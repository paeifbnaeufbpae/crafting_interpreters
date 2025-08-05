function fib(n) {
  if (n < 2) return n;
  return fib(n - 1) + fib(n - 2);
}

const before = performance.now();
console.log(fib(40));
const after = performance.now();
console.log((after - before) / 1000);
def fibonacci(n):
    if n <= 0:
        return []
    elif n == 1:
        return [0]
    elif n == 2:
        return [0, 1]
    else:
        fib_sequence = [0, 1]
        while len(fib_sequence) < n:
            next_num = fib_sequence[-1] + fib_sequence[-2]
            fib_sequence.append(next_num)
        return fib_sequence

def sqrt(x):
    return x ** 0.5

def factorial(n):
    if n == 0:
        return 1
    else:
        return n * factorial(n - 1)
    
    
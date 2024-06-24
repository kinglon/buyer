# 大整数运算
class BigIntUtil:
    # 2个大整数相乘
    @staticmethod
    def bigint_multiple(number1, number2):
        bigint1 = int.from_bytes(number1, 'big')  # convert bytes to integer
        bigint2 = int.from_bytes(number2, 'big')  # convert bytes to integer
        result = bigint1 * bigint2
        result_bytes = result.to_bytes((result.bit_length() + 7) // 8, 'big')  # convert result back to bytes
        return result_bytes

    # 2个大整数相加
    @staticmethod
    def bigint_add(number1, number2):
        bigint1 = int.from_bytes(number1, 'big')  # convert bytes to integer
        bigint2 = int.from_bytes(number2, 'big')  # convert bytes to integer
        result = bigint1 + bigint2
        result_bytes = result.to_bytes((result.bit_length() + 7) // 8, 'big')  # convert result back to bytes
        return result_bytes

    # 2个大整数相减
    @staticmethod
    def bigint_subtract(number1, number2):
        bigint1 = int.from_bytes(number1, 'big')  # convert bytes to integer
        bigint2 = int.from_bytes(number2, 'big')  # convert bytes to integer
        result = bigint1 - bigint2
        result_bytes = result.to_bytes((result.bit_length() + 7) // 8, 'big')  # convert result back to bytes
        return result_bytes

    # 2个大整数求余
    @staticmethod
    def bigint_mod(number1, number2):
        bigint1 = int.from_bytes(number1, 'big')  # convert bytes to integer
        bigint2 = int.from_bytes(number2, 'big')  # convert bytes to integer
        result = bigint1 % bigint2
        result_bytes = result.to_bytes((result.bit_length() + 7) // 8, 'big')  # convert result back to bytes
        return result_bytes

    # 2个大整数求幂
    @staticmethod
    def bigint_power(number1, number2):
        bigint1 = int.from_bytes(number1, 'big')  # convert bytes to integer
        bigint2 = int.from_bytes(number2, 'big')  # convert bytes to integer
        result = bigint1 ^ bigint2
        result_bytes = result.to_bytes((result.bit_length() + 7) // 8, 'big')  # convert result back to bytes
        return result_bytes

    # 大整数求幂再求余
    @staticmethod
    def bigint_powmod(t, r, e):
        t = int.from_bytes(t, 'big')
        r = int.from_bytes(r, 'big')
        e = int.from_bytes(e, 'big')

        if e == 1:
            return 0
        n = 1
        t %= e
        while r > 0:
            if r % 2 == 1:
                n = (n * t) % e
            r //= 2
            t = (t * t) % e
        n_bytes = n.to_bytes((n.bit_length() + 7) // 8, 'big')  # c
        return n_bytes

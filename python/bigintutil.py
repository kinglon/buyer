# 大整数运算
class BigIntUtil:
    # 大整数求幂再求余
    @staticmethod
    def bigint_powmod(t, r, e):
        if e == 1:
            return 0
        n = 1
        t %= e
        while r > 0:
            if r % 2 == 1:
                n = (n * t) % e
            r //= 2
            t = (t * t) % e
        return n

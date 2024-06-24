import requests
import json
import base64
import hashlib
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import padding
from bigintutil import BigIntUtil


class AppleUtil:
    def __init__(self):
        self.auth_host = 'https://idmsa.apple.com'
        self.proxies = {"http": "", "https": ""}

    def get_common_request_header(self):
        headers = {
            'Content-Type': 'application/json',
            'Accept': 'application/json, text/plain, */*',
            'Accept-Encoding': 'gzip, deflate, br, zstd',
            'Origin': self.auth_host,
            'Referer': self.auth_host,
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36',

            'X-Apple-Auth-Attributes': 'JC2sm29mAKAQpv8h2UndqU+Dr4KalzfSlwHzW4HuAhcLKi7ea7bwIgBYesiL31YpGdPjmHunm+c9EopjIo77E8fpg7cKgIBD+agnH5gk002t84xvcmHD4q7qiVnqmkc4KBCoEGNCNj+BkZRszh9HS/dtaEL0rAkxjENbrch07jKOBA+blVYx0fEWXkSoKEZaHWO8t1L8ks9Gwf2rqzl8JwhbAuGXz6qczr2H1V5+NvtBBhAH85ozaFopgRZSGGy2iGAS3EsvQfDhxQcAFSS0oh2fiQ==',
            'X-Apple-Domain-Id': '39',
            'X-Apple-Frame-Id': 'auth-o7ggaaqz-mnv4-nsy1-58wj-qixcoisc',
            'X-Apple-I-Fd-Client-Info': '{"U":"Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36","L":"zh-CN","Z":"GMT+08:00","V":"1.1","F":"Fla44j1e3NlY5BNlY5BSmHACVZXnNA9LL30OIFx_HrurJhBR.uMp4UdHz13Nlxfs.xLB.Tf1cK0DBRa6mmjoaUaW5BNlY5CGWY5BOgkLT0XxU..5GG"}',
            'X-Apple-Locale': 'ja_JP',
            'X-Apple-Oauth-Client-Id': 'a797929d224abb1cc663bb187bbcd02f7172ca3a84df470380522a7c6092118b',
            'X-Apple-Oauth-Client-Type': 'firstPartyAuth',
            'X-Apple-Oauth-Redirect-Uri': 'https://secure6.store.apple.com',
            'X-Apple-Oauth-Response-Mode': 'web_message',
            'X-Apple-Oauth-Response-Type': 'code',
            'X-Apple-Oauth-State': 'auth-o7ggaaqz-mnv4-nsy1-58wj-qixcoisc',
            'X-Apple-Widget-Key': 'a797929d224abb1cc663bb187bbcd02f7172ca3a84df470380522a7c6092118b'
        }
        return headers

    # 获取private和public值, 都是bytes
    @staticmethod
    def get_private_public_value():
        magic_value = bytes(
            [172, 107, 219, 65, 50, 74, 154, 155, 241, 102, 222, 94, 19, 137, 88, 47, 175, 114, 182, 101, 25, 135,
             238, 7,
             252, 49, 146, 148, 61, 181, 96, 80, 163, 115, 41, 203, 180, 160, 153, 237, 129, 147, 224, 117, 119,
             103, 161,
             61, 213, 35, 18, 171, 75, 3, 49, 13, 205, 127, 72, 169, 218, 4, 253, 80, 232, 8, 57, 105, 237, 183,
             103, 176,
             207, 96, 149, 23, 154, 22, 58, 179, 102, 26, 5, 251, 213, 250, 170, 232, 41, 24, 169, 150, 47, 11, 147,
             184,
             85, 249, 121, 147, 236, 151, 94, 234, 168, 13, 116, 10, 219, 244, 255, 116, 115, 89, 208, 65, 213, 195,
             62,
             167, 29, 40, 30, 68, 107, 20, 119, 59, 202, 151, 180, 58, 35, 251, 128, 22, 118, 189, 32, 122, 67, 108,
             100,
             129, 241, 210, 185, 7, 135, 23, 70, 26, 91, 157, 50, 230, 136, 248, 119, 72, 84, 69, 35, 181, 36, 176,
             213,
             125, 94, 167, 122, 39, 117, 210, 236, 250, 3, 44, 251, 219, 245, 47, 179, 120, 97, 96, 39, 144, 4, 229,
             122,
             230, 175, 135, 78, 115, 3, 206, 83, 41, 156, 204, 4, 28, 123, 195, 8, 216, 42, 86, 152, 243, 168, 208,
             195,
             130, 113, 174, 53, 248, 233, 219, 251, 182, 148, 181, 200, 3, 216, 159, 122, 228, 53, 222, 35, 109, 82,
             95, 84,
             117, 155, 101, 227, 114, 252, 214, 142, 242, 15, 167, 17, 31, 158, 74, 255, 115])
        private_value = bytes(
            [187, 190, 210, 164, 71, 19, 67, 236, 35, 91, 123, 152, 211, 101, 163, 141, 128, 113, 150, 105, 113, 83,
             114, 184, 16, 202, 55, 63, 109, 166, 144, 159, 46, 192, 243, 182, 135, 207, 64, 206, 163, 12, 168, 249,
             173, 235, 159, 102, 109, 177, 225, 110, 49, 144, 74, 99, 250, 71, 185, 150, 35, 200, 104, 151, 115, 51,
             61, 161, 77, 53, 177, 157, 4, 206, 138, 224, 223, 72, 201, 175, 247, 205, 79, 39, 199, 244, 219, 203,
             169, 68, 83, 124, 39, 78, 226, 35, 23, 214, 134, 241, 146, 53, 115, 191, 221, 14, 24, 68, 177, 52, 17,
             94, 249, 86, 143, 162, 4, 218, 118, 41, 207, 225, 2, 91, 110, 122, 231, 255, 82, 75, 175, 218, 193,
             244, 21, 52, 138, 152, 34, 97, 188, 41, 169, 203, 82, 103, 228, 142, 170, 245, 87, 244, 62, 169, 217,
             206, 163, 211, 220, 21, 180, 34, 135, 255, 201, 119, 43, 225, 177, 79, 161, 159, 31, 147, 113, 48, 55,
             226, 8, 112, 252, 70, 251, 29, 236, 75, 58, 85, 241, 205, 137, 151, 72, 166, 255, 134, 35, 99, 116,
             141, 237, 17, 3, 89, 83, 46, 70, 76, 85, 246, 223, 53, 205, 156, 45, 152, 62, 176, 62, 110, 156, 128,
             116, 246, 143, 31, 241, 167, 255, 174, 126, 168, 132, 97, 238, 162, 183, 202, 11, 201, 223, 186, 59,
             190, 49, 149, 171, 145, 196, 162, 13, 122, 10, 77, 81, 47])
        public_value = BigIntUtil.bigint_powmod(bytes([2]), private_value, magic_value)
        return [private_value, public_value]

    # 登录前的初始化
    # public_value, base64串
    # account_name, 账号
    # 返回：
    # {
    #     "iteration": 20173,
    #     "salt": "SInpSF5/lsMlDU5xPZIa8w==",
    #     "protocol": "s2k",
    #     "b": "VQmWgVyEjNb/tSPcslrO6UF2JMYLolrS1jYyTCd5v8CaX03fG8HbktmVpZ7Iztfxsuwqs6p2L6QFV4yBHfjkphGjq+D/JwyxZwkJe5bHjP3RB4cx7IrPnJNe0LaG41+3v95A8j0F20hEBikHDdroSiLQ7Hw3JO9N7hZaensnAi15NfTNGPTcm4gWAQwU/bQBhDRsM1mp1iGgRJmXZkPF9IpvA60FpygkCxgpvgYMpmy+YzO1OFpbi/172TLrFB5twZoXVAuuKC5GsGVeH8WbUkwHbDg1znjvaidVP3wS8VRSJFu78Bi7YvMWvYc28AvCjULh6nBoxekbRkR32An2zg==",
    #     "c": "d-444-88ff3978-31f9-11ef-bc4e-eb1aee3d6c79:MSA"
    # }
    def auth_init(self, public_value, account_name):
        try:
            uri = '/appleauth/auth/signin/init'
            url = self.auth_host + uri
            headers = self.get_common_request_header()
            body = {"a": public_value,
                    "accountName": account_name,
                    "protocols": ["s2k"]
                    }
            body = json.dumps(body)
            response = requests.post(url, headers=headers, data=body, proxies=self.proxies)
            if not response.ok:
                print("failed to do auth init, error is {}".format(response))
                return None
            else:
                data = response.content.decode('utf-8')
                # print("init data: {}".format(data))
                data = json.loads(data)
                return data
        except requests.exceptions.RequestException as e:
            print("failed to do auth init, error is {}".format(e))
            return None

    # 登录第2阶段
    # 返回账号信息，从Set-Cookie的myacinfo中获得
    def auth_complete(self, account_name, c, m1, m2):
        try:
            uri = '/appleauth/auth/signin/complete?isRememberMeEnabled=true'
            url = self.auth_host + uri
            headers = self.get_common_request_header()
            headers['X-APPLE-HC'] = '1:10:20240531133108:3d680cab1c02717f70c7b4a9617dee12::56'
            body = {"c": c,
                    "accountName": account_name,
                    "m1": m1,
                    "m2": m2,
                    "rememberMe": False
                    }
            body = json.dumps(body)
            response = requests.post(url, headers=headers, data=body, proxies=self.proxies)
            if not response.ok:
                print("failed to do auth complete, error is {}".format(response))
                return None
            else:
                data = response.content.decode('utf-8')
                data = json.loads(data)
                if 'authType' in data:
                    # 从Set-Cookie中获取账号信息
                    cookies = response.headers.get('Set-Cookie')
                    begin = cookies.find('myacinfo=')
                    if begin >= 0:
                        begin += len('myacinfo=')
                        end = cookies.find(';', begin)
                        return cookies[begin:end]
                return None
        except requests.exceptions.RequestException as e:
            print("failed to do auth complete, error is {}".format(e))
            return None

    # 登录，分2个阶段：获取初始化数据，再登录
    # 返回账号信息，从Set-Cookie的myacinfo中获得
    def login(self, account_name, password):
        # 获取初始化数据
        private_value, public_value = AppleUtil.get_private_public_value()
        init_data = self.auth_init(AppleUtil.btoa(public_value), account_name)
        if not init_data:
            return False

        server_public_value = init_data['b']
        salt = init_data['salt']
        iteration = init_data['iteration']
        c = init_data['c']
        m1, m2 = self.get_complete_data(account_name, password, salt, iteration, private_value, public_value,
                                        server_public_value)

        login_data = self.auth_complete(account_name, c, m1, m2)
        return login_data

    def add_cart(self):
        try:
            url = 'https://securemetrics.apple.com/b/ss/applestoreww/1/JS-2.23.0/s85145633119268?AQB=1&ndh=1&pf=1&t=24%2F5%2F2024%2020%3A54%3A5%201%20-480&fid=7790B7456A6939C4-2F6AD5CFED9B4308&ce=UTF-8&cdp=2&cl=1800&pageName=AOS%3A%20home%2Fshop_iphone%2Ffamily%2Fiphone_15_pro%2Fselect&g=https%3A%2F%2Fwww.apple.com%2Fjp%2Fshop%2Fbuy-iphone%2Fiphone-15-pro%2F6.1%25E3%2582%25A4%25E3%2583%25B3%25E3%2583%2581%25E3%2583%2587%25E3%2582%25A3%25E3%2582%25B9%25E3%2583%2597%25E3%2583%25AC%25E3%2582%25A4-512gb-%25E3%2583%258A%25E3%2583%2581%25E3%2583%25A5%25E3%2583%25A9%25E3%2583%25AB%25E3%2583%2581%25E3%2582%25BF%25E3%2583%258B%25E3%2582%25A6%25E3%2583%25A0-sim%25E3%2583%2595%25E3%2583&r=https%3A%2F%2Fwww.apple.com%2Fjp%2Fiphone-15-pro%2F&cc=JPY&server=as-26.5.10&events=scAdd&products=iphone_15_pro%3BMTUK3%3B1%3B204800.00%3B%3B&h1=aos%3Ashop&l1=D%3Das_xs&v3=AOS%3A%20Japan%20Consumer&l3=D%3Das_tex&c4=D%3Dg&v4=D%3DpageName&c5=win32&v5=D%3DpageName%2B%22%7C%7CStep%201%7C%E3%83%90%E3%83%83%E3%82%B0%E3%81%AB%E8%BF%BD%E5%8A%A0%22&v6=D%3DpageName%2B%22%7C%7CStep%201%7C6_1INCH%20%3E%20NATURALTITANIUM%20%3E%20512GB%20%3E%20UNLOCKED_JP%20%3E%20TRADE_IN_NO%20%3E%20FULLPRICE%7Cfinal%22&c8=AOS%3A%20iPhone&c14=AOS%3A%20home%2Fshop_iphone%2Ffamily%2Fiphone_15_pro%2Fattach&v14=ja-jp&c20=AOS%3A%20JP%20Consumer&c40=10147&v45=iphone%3Aiphone_15_pro_6_1inch%3Enaturaltitanium%3E512gb%3Eno%20tradein%3Efullprice%3Eunlocked_jp%3Eapplecare%3Ano&v49=D%3Dr&v54=D%3Dg&v97=s.tl-o&pe=lnk_o&pev2=Step%201&c.&a.&activitymap.&page=AOS%3A%20home%2Fshop_iphone%2Ffamily%2Fiphone_15_pro%2Fselect&link=%28inner%20text%29%20%7C%20no%20href%20%7C%20summary&region=summary&pageIDType=1&.activitymap&.a&.c&s=1366x768&c=24&j=1.6&v=N&k=Y&bw=427&bh=607&-g=%25AA%25E3%2583%25BC&AQE=1'
            headers = self.get_common_request_header()
            headers['Cookie'] = 'dssid2=e1737f88-6d40-4801-b25a-249cdcd3de05; dssf=1; pxro=1; as_sfa=MnxqcHxqcHx8amFfSlB8Y29uc3VtZXJ8aW50ZXJuZXR8MHwwfDE; as_uct=0; as_disa=AAAjAAABm1laUDouQaF5TP72s6sZZj8FbCJodyOTaVj-NAN9VPFR6aBeoOu6O95xTIMYtrzpAAIBi9GbcNf7GtpLiDSxXLO4xP3x0CeumpUz8i2IHHtDo8o=; geo=CN; s_cc=true; as_pcts=jNuHzXKS6tpkZKHRVVd8JLTeeOH-+gX4co-fZNKSNG2Q_4crOpa4Uijq2TkC-gSM:3EAlyCS9q_pR:v5_SMX4sa8BLoCO2zwl8-AmXJogHxvPjWL6t9vTvylepb8nFkNtmurgErXCAceAe1fjffIcNlg3ZU+w93FPVqkI4af; at_check=true; as_tex=~1~|564632:3:1726327369:JPN|3u/Ps/zhMTCjUD+k/xq/2RkV6d/UNA+XO4EGu52WCJE; as_rumid=5100dfc1-82a9-4f59-af48-a9933ccb158c; s_fid=7790B7456A6939C4-2F6AD5CFED9B4308; as_dc=ucp3; s_vi=[CS]v1|333CB525A64765B7-400006846510FD7E[CE]; s_sq=%5B%5BB%5D%5D'
            response = requests.post(url, headers=headers, proxies=self.proxies)
            if not response.ok:
                print("failed to add cart, error is {}".format(response))
                return False
            else:
                return True
        except requests.exceptions.RequestException as e:
            print("failed to add cart, error is {}".format(e))
            return False

    def check_credit_card(self, card_number):
        try:
            url = 'https://secure8.store.apple.com/jp/shop/checkoutx/billing?_a=checkCreditCardTypeAction&_m=checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0'
            headers = {'Content-Type': 'application/x-www-form-urlencoded',
                       'Accept': 'application/json, text/plain, */*', 'Accept-Encoding': 'gzip, deflate, br, zstd',
                       'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36',
                       'Origin': 'https://secure8.store.apple.com',
                       'Referer': 'https://secure8.store.apple.com/jp/shop/checkout?_s=Billing-init',
                       'Cookie': 'dssid2=e1737f88-6d40-4801-b25a-249cdcd3de05; dssf=1; as_tex=~1~|547472:1:1720303320:USA|8tl/pg7UrFynAkX6Y0Pvc49nI15a+Q7a4p8Rus7Z8rQ; pxro=1; as_sfa=MnxqcHxqcHx8amFfSlB8Y29uc3VtZXJ8aW50ZXJuZXR8MHwwfDE; as_uct=0; geo=CN; s_cc=true; as_pcts=TGihRP6latlhmOcem2AvgTWVxgiKU7s7yVNw-lyMoufrrCRlYgixazjPZjuyYYmxXadPpicVE7DkSepOZEvgZjxoPgnfW1Msjo90mEHBfUyENobs+T1wjCLHCs-VNwILA4wrUNfvwlQQsm2PP7sW3cp47qwDfUa0p4t_uB2I; at_check=true; as_rumid=936b6e56-bea2-442d-bd5e-c856941de6e8; dslang=JP-JA; site=JPN; as_cn=~7TA2AxBhMPnrepuXgX5Rx5omb7TuLmtGvFs8KSqk-ME=; as_loc=c82ad3b931014cf632200f5f0367479192c5dbc172db3e6954fb0896c2140a3a5a936666b546c9833bb6f386a5dece332cfe086faae0de548059089372f46494d1b4dc5c7da9a47c47be2c8bcf532be96e8457a1871a2d8682aed2ec65d3797b3ab3ed2d453b603ec66210f37f77d5c4; rtsid=%7BJP%3D%7Bt%3Da%3Bi%3DR079%3B%7D%3B%7D; as_dc=ucp5; s_fid=1355A9866141D8FA-31F8EA28D2C3B466; s_vi=[CS]v1|332E34868CD8D581-60001418254370F3[CE]; as_disa=AAAjAAABKY54Eci6DGrDTJd9nXI1YATavCZE640dkzcAvsu1y4uH8l-XwueWZ0zyjWaVvB4GAAIBbeMv8ihKB98KIMjMI-m0z1aHP5CXTVFP4cz6BGSmZac=; as_rec=9657cbef12fb9b072fba16246736ce1e544f9f7215ce76e7f18dce4eafc41ad5dc0de7f643945d1fa1baa23b74e635e3380c7d0c0fe2077adaf22133482afc0482cb98930474e01b0078311122a31047; as_ltn_jp=AAQEAMAsYfz8j_u80nWm9JxszbmOBX8h3QGr35CalH8kv4tvAPd6qxJ3-vVO54NxLUv4KChSpoGYs2JUS-lQO7agi9BZ8ycuTLg; s_sq=applestoreww%3D%2526c.%2526a.%2526activitymap.%2526page%253DAOS%25253A%252520Checkout%25252FPayment%2526link%253D159%25252C800visa%25252C%252520mastercard%25252C%252520amex%25252C%252520%25252C%252520jcb%252520%252528inner%252520text%252529%252520%25257C%252520no%252520href%252520%25257C%252520body%2526region%253Dbody%2526pageIDType%253D1%2526.activitymap%2526.a%2526.c%2526pid%253DAOS%25253A%252520Checkout%25252FPayment%2526pidt%253D1%2526oid%253Dfunctionkd%252528%252529%25257B%25257D%2526oidt%253D2%2526ot%253DDIV',
                       'Modelversion': 'v2', 'x-aos-model-page': 'checkoutPage',
                       'x-aos-stk': 'QVkzS5yCLePx_y06-GHMzaOi-4M', 'X-Requested-With': 'Fetch'}
            # Cookie是来自于fulfillment请求的SetCookie
            # headers['Cookie'] = 's_vi=[CS]v1|332E34868CD8D581-60001418254370F3[CE];dssid2=e1737f88-6d40-4801-b25a-249cdcd3de05'
            # headers['Syntax'] = 'graviton'
            public_key = 'MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAvUIrYPRsCjQNCEGNWmSp9Wz+5uSqK6nkwiBq254Q5taDOqZz0YGL3s1DnJPuBU+e8Dexm6GKW1kWxptTRtva5Eds8VhlAgph8RqIoKmOpb3uJOhSzBpkU28uWyi87VIMM2laXTsSGTpGjSdYjCbcYvMtFdvAycfuEuNn05bDZvUQEa+j9t4S0b2iH7/8LxLos/8qMomJfwuPwVRkE5s5G55FeBQDt/KQIEDvlg1N8omoAjKdfWtmOCK64XZANTG2TMnar/iXyegPwj05m443AYz8x5Uw/rHBqnpiQ4xg97Ewox+SidebmxGowKfQT3+McmnLYu/JURNlYYRy2lYiMwIDAQAB'
            cipher_text = AppleUtil.encrypt(public_key, '5353074484065468'.encode('utf-8'))
            body = 'checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.validCardNumber=true&checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.cardNumberForBinDetection=5353%2007&checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.selectCardType=MASTERCARD&checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.securityCode=&checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.expiration=&checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.cardNumber=%7B%22cipherText%22%3A%22{}%3D%3D%22%2C%22publicKeyHash%22%3A%22DsCuZg%2B6iOaJUKt5gJMdb6rYEz9BgEsdtEXjVc77oAs%3D%22%7D'.format(
                cipher_text)
            response = requests.post(url, headers=headers, data=body, proxies=self.proxies)
            # s = requests.Session()
            # s.mount('https://', requests.adapters.HTTPAdapter(max_retries=1))
            #
            # req = requests.Request('POST', url, headers = headers, data = body)
            # prep = req.prepare()
            # response = s.send(prep)
            if not response.ok:
                print("failed to do auth federate, error is {}".format(response))
                return False
            else:
                data = response.content.decode('utf-8')
                data = json.loads(data)
                return data
        except requests.exceptions.RequestException as e:
            print("failed to get the demand list, error is {}".format(e))
            return False

    # bytes进行base64编码
    @staticmethod
    def btoa(binary_data):
        return base64.b64encode(binary_data).decode('ascii')

    # rsa加密，public_key是base64串， text是bytes
    @staticmethod
    def encrypt(public_key, text):
        public_key = base64.b64decode(public_key)
        public_key1 = serialization.load_der_public_key(public_key)
        ciphertext = public_key1.encrypt(
            text,
            padding.OAEP(
                mgf=padding.MGF1(algorithm=hashes.SHA256()),
                algorithm=hashes.SHA256(),
                label=None
            )
        )
        ciphertext = base64.b64encode(ciphertext).decode('utf-8')
        return ciphertext

    # 获取M1, M2值
    @staticmethod
    def get_complete_data(account, password, salt, iteration, private_value, public_value, server_public_value):
        # 计算account
        account = account.lower().encode('utf-8')
        account_hash = hashlib.sha256(account).digest()

        # 计算密码哈希值
        password = password.encode('utf-8')
        password = hashlib.sha256(password).digest()

        # 获取salt内容
        salt = salt.encode('utf-8')
        salt = base64.decodebytes(salt)

        # 获取server_public_value
        server_public_value = server_public_value.encode('utf-8')
        server_public_value = base64.decodebytes(server_public_value)

        # 使用PBKDF2算法计算秘钥
        kdf = PBKDF2HMAC(
            algorithm=hashes.SHA256(),
            length=32,
            salt=salt,
            iterations=iteration,
        )
        key = kdf.derive(password)

        # 计算这个固定串的hash
        magic_value = bytes(
            [172, 107, 219, 65, 50, 74, 154, 155, 241, 102, 222, 94, 19, 137, 88, 47, 175, 114, 182, 101, 25, 135, 238,
             7, 252, 49, 146, 148, 61, 181, 96, 80, 163, 115, 41, 203, 180, 160, 153, 237, 129, 147, 224, 117, 119, 103,
             161, 61, 213, 35, 18, 171, 75, 3, 49, 13, 205, 127, 72, 169, 218, 4, 253, 80, 232, 8, 57, 105, 237, 183,
             103, 176, 207, 96, 149, 23, 154, 22, 58, 179, 102, 26, 5, 251, 213, 250, 170, 232, 41, 24, 169, 150, 47,
             11, 147, 184, 85, 249, 121, 147, 236, 151, 94, 234, 168, 13, 116, 10, 219, 244, 255, 116, 115, 89, 208, 65,
             213, 195, 62, 167, 29, 40, 30, 68, 107, 20, 119, 59, 202, 151, 180, 58, 35, 251, 128, 22, 118, 189, 32,
             122, 67, 108, 100, 129, 241, 210, 185, 7, 135, 23, 70, 26, 91, 157, 50, 230, 136, 248, 119, 72, 84, 69, 35,
             181, 36, 176, 213, 125, 94, 167, 122, 39, 117, 210, 236, 250, 3, 44, 251, 219, 245, 47, 179, 120, 97, 96,
             39, 144, 4, 229, 122, 230, 175, 135, 78, 115, 3, 206, 83, 41, 156, 204, 4, 28, 123, 195, 8, 216, 42, 86,
             152, 243, 168, 208, 195, 130, 113, 174, 53, 248, 233, 219, 251, 182, 148, 181, 200, 3, 216, 159, 122, 228,
             53, 222, 35, 109, 82, 95, 84, 117, 155, 101, 227, 114, 252, 214, 142, 242, 15, 167, 17, 31, 158, 74, 255,
             115])
        magic_value_hash = hashlib.sha256(magic_value).digest()
        magic_value_padding = bytes(
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 2])
        magic_value_padding_hash = hashlib.sha256(magic_value + magic_value_padding).digest()

        # 冒号+key计算hash值
        key2 = hashlib.sha256(':'.encode('utf-8')+key).digest()

        # salt+key2计算哈希
        key3 = hashlib.sha256(salt+key2).digest()

        # public value+server public value计算哈希值
        public_value_hash = hashlib.sha256(public_value+server_public_value).digest()

        bigint_l = BigIntUtil.bigint_add(BigIntUtil.bigint_multiple(public_value_hash, key3), private_value)
        bigint_p = BigIntUtil.bigint_mod(BigIntUtil.bigint_multiple(BigIntUtil.bigint_powmod([2], key3, magic_value), magic_value_padding_hash), magic_value)
        bigint_v = BigIntUtil.bigint_powmod(BigIntUtil.bigint_mod(BigIntUtil.bigint_subtract(server_public_value, bigint_p), magic_value), bigint_l, magic_value)
        x = hashlib.sha256(bigint_v).digest()

        padding_bytes = bytes(
            [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
             0, 0, 0, 2])
        padding_hash = hashlib.sha256(padding_bytes).digest()  # s
        bigint_h = BigIntUtil.bigint_power(magic_value_hash, padding_hash)
        m = hashlib.sha256(bigint_h+account_hash+salt+public_value+server_public_value+x).digest()
        m1 = AppleUtil.btoa(m)
        m2 = AppleUtil.btoa(hashlib.sha256(public_value+m+x).digest())

        print('M1: {}'.format(m1))
        print('M2: {}'.format(m2))

        # e = iteration
        # l = e
        # a = password
        # f = public_value
        # u = 's2k'
        # c = 's2k'
        # v = account  # 账号转成小写后utf-8编码
        # o = salt
        # p = salt
        # d = key  # 密码串生成的key
        # y = key3
        # b = public_value_hash
        # s = private_value
        # h = server_public_value
        # g = magic_value_padding_hash
        # j = magic_value
        # D = '02'
        # w = bigint_v
        return m1, m2


def test_encrypt():
    public_key = 'MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAvUIrYPRsCjQNCEGNWmSp9Wz+5uSqK6nkwiBq254Q5taDOqZz0YGL3s1DnJPuBU+e8Dexm6GKW1kWxptTRtva5Eds8VhlAgph8RqIoKmOpb3uJOhSzBpkU28uWyi87VIMM2laXTsSGTpGjSdYjCbcYvMtFdvAycfuEuNn05bDZvUQEa+j9t4S0b2iH7/8LxLos/8qMomJfwuPwVRkE5s5G55FeBQDt/KQIEDvlg1N8omoAjKdfWtmOCK64XZANTG2TMnar/iXyegPwj05m443AYz8x5Uw/rHBqnpiQ4xg97Ewox+SidebmxGowKfQT3+McmnLYu/JURNlYYRy2lYiMwIDAQAB'
    encrypt_data = AppleUtil.encrypt(public_key, '5353079623725781'.encode('utf-8'))
    print(encrypt_data)


def test_login():
    apple_util = AppleUtil()
    account_info = apple_util.login('ii5pfw9mvfw9i@163.com', 'Qf223322')
    print(account_info)


def test_add_cart():
    apple_util = AppleUtil()
    apple_util.add_cart()


def test_order():
    apple_util = AppleUtil()
    apple_util.check_credit_card('5353079623725781')


if __name__ == '__main__':
    # test_encrypt()
    # test_login()
    test_add_cart()
    # test_order()
    print('done')

import requests
import json
import base64
import hashlib
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.kdf.pbkdf2 import PBKDF2HMAC
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import padding
from bigintutil import BigIntUtil
from datamodel import DataModel
import urllib.parse


class AppleUtil:
    def __init__(self):
        self.auth_host = 'https://idmsa.apple.com'
        self.apple_host = 'https://www.apple.com/jp'
        self.appstore_host = 'https://secure5.store.apple.com/jp'
        self.proxies = {"http": "", "https": ""}
        self.cookies = {}
        self.debug_cookie = ''

    # 保存响应内容，调试使用
    @staticmethod
    def debug_save_response(response):
        content = response.content.decode('utf-8')
        with open(r'C:\Users\zengxiangbin\Downloads\cart.html', 'w', encoding='utf-8') as f:
            f.write(content)

    def get_common_request_header(self):
        if len(self.debug_cookie) > 0:
            cookies = self.debug_cookie
        else:
            cookies = '; '.join(f'{key}={value}' for key, value in self.cookies.items())
        headers = {
            'Content-Type': 'application/json',
            'Accept': 'application/json, text/plain, */*',
            'Accept-Encoding': 'gzip, deflate, br, zstd',
            'Origin': self.apple_host,
            'Referer': self.apple_host,
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/121.0.0.0 Safari/537.36',
            'Cookie': cookies
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
                self.cookies.update(response.cookies.get_dict())
                data = response.content.decode('utf-8')
                data = json.loads(data)
                return data
        except requests.exceptions.RequestException as e:
            print("failed to do auth init, error is {}".format(e))
            return None

    # 登录第2阶段
    # 返回 True or False
    def auth_complete(self, account_name, c, m1, m2):
        try:
            uri = '/appleauth/auth/signin/complete?isRememberMeEnabled=true'
            url = self.auth_host + uri
            headers = self.get_common_request_header()
            headers['X-Apple-Oauth-Client-Type'] = 'firstPartyAuth'
            headers['X-Apple-Oauth-Response-Mode'] = 'web_message'
            headers['X-Apple-Oauth-Response-Type'] = 'code'
            headers['X-Apple-Widget-Key'] = 'a797929d224abb1cc663bb187bbcd02f7172ca3a84df470380522a7c6092118b'
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
                cookies = response.cookies.get_dict()
                if 'myacinfo' in cookies:
                    return cookies['myacinfo']
                else:
                    return None
        except requests.exceptions.RequestException as e:
            print("failed to do auth complete, error is {}".format(e))
            return None

    # 登录，分2个阶段：获取初始化数据，再登录
    # 返回 myacinfo 的 cookie值
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

        return self.auth_complete(account_name, c, m1, m2)

    # 添加商品到购物车
    # model 型号，如：iphone-15-pro
    # product 产品，如：MTU93J(iphone 15 pro max, 1T, 天然钛)  MU713J(iphone 15 pro, 128G, 天然钛)
    def add_cart(self, model, product):
        try:
            # 获取atb token
            url = self.apple_host + '/shop/beacon/atb'
            headers = self.get_common_request_header()
            response = requests.get(url, headers=headers, proxies=self.proxies)
            if not response.ok:
                print("failed to update cookies when adding cart, error is {}".format(response))
                return False
            else:
                cookies = response.cookies.get_dict()
                if 'as_atb' not in cookies:
                    return False
                else:
                    if 'as_dc' in cookies:
                        cookies['as_dc'] = 'ucp2'
                    self.cookies.update(cookies)

            # 加入购物车
            atbtoken = self.cookies['as_atb']
            atbtoken = atbtoken[atbtoken.rfind('|')+1:]
            url = (self.apple_host + '/shop/buy-iphone/{}?product={}%2FA&purchaseOption=fullPrice&cppart=UNLOCKED_JP&step=select&ao.applecare_58=none&ao.applecare_58_theft_loss=none&ams=0&atbtoken={}&igt=true&add-to-cart=add-to-cart'
                   .format(model, product, atbtoken))
            headers = self.get_common_request_header()
            response = requests.get(url, headers=headers, proxies=self.proxies)
            if not response.ok:
                print("failed to add cart, error is {}".format(response))
                return False
            else:
                self.cookies.update(response.cookies.get_dict())
                return True
        except requests.exceptions.RequestException as e:
            print("failed to add cart, error is {}".format(e))
            return False

    # 查询是否有货
    # location 邮编，如：104-8125
    # product 产品，如：MTU93J/A(iphone 15 pro max, 1T, 天然钛)  MU713J/A(iphone 15 pro, 128G, 天然钛)
    def query_product_available(self, location, product):
        try:
            url = (self.apple_host + '/shop/fulfillment-messages?pl=true&mts.0=regular&cppart=UNLOCKED_JP&parts.0={}/A&location={}'
                   .format(product, location))
            headers = self.get_common_request_header()
            response = requests.get(url, headers=headers, proxies=self.proxies)
            if not response.ok:
                print("failed to query product available, error is {}".format(response))
                return False
            else:
                data = response.content.decode('utf-8')
                root = json.loads(data)
                stores = root['body']['content']['pickupMessage']['stores']
                for store in stores:
                    if store['retailStore']['availableNow']:
                        return True
                return False
        except requests.exceptions.RequestException as e:
            print("failed to query product available, error is {}".format(e))
            return False

    # 打开购物车
    # 返回x-aos-stk的值
    def open_cart(self):
        try:
            url = self.apple_host + '/shop/bag'
            headers = self.get_common_request_header()
            response = requests.get(url, headers=headers, proxies=self.proxies)
            if not response.ok:
                print("failed to open cart, error is {}".format(response))
                return None
            else:
                self.cookies.update(response.cookies.get_dict())
                data = response.content.decode('utf-8')
                begin = data.find('x-aos-stk":')
                if begin > 0:
                    begin = data.find('"', begin + len('x-aos-stk":'))
                    if begin > 0:
                        end = data.find('"', begin + 1)
                        x_aos_stk = data[begin+1: end]
                        return x_aos_stk
                return None
        except requests.exceptions.RequestException as e:
            print("failed to open cart, error is {}".format(e))
            return None

    # 进入下单流程
    # 返回ssi
    def check_now(self, x_aos_stk):
        try:
            url = self.apple_host + '/shop/bagx/checkout_now?_a=checkout&_m=shoppingCart.actions'
            headers = self.get_common_request_header()
            headers['Content-Type'] = 'application/x-www-form-urlencoded'
            headers['X-Aos-Stk'] = x_aos_stk
            headers['X-Requested-With'] = 'Fetch'
            headers['X-Aos-Model-Page'] = 'cart'
            body = 'shoppingCart.recommendations.recommendedItem.part=&shoppingCart.bagSavedItems.part=&shoppingCart.bagSavedItems.listId=&shoppingCart.bagSavedItems.childPart=&shoppingCart.items.item-47e7a306-7cfd-4722-b440-1b6ba32b1647.isIntentToGift=false&shoppingCart.items.item-47e7a306-7cfd-4722-b440-1b6ba32b1647.itemQuantity.quantity=1&shoppingCart.locationConsent.locationConsent=false&shoppingCart.summary.promoCode.promoCode=&shoppingCart.actions.fcscounter=&shoppingCart.actions.fcsdata='
            response = requests.post(url, headers=headers, data=body, proxies=self.proxies)
            if not response.ok:
                print("failed to check now, error is {}".format(response))
                return None
            else:
                data = response.content.decode('utf-8')
                root = json.loads(data)
                url = root['head']['data']['url']
                if url.find('ssi=') != -1:
                    self.cookies.update(response.cookies.get_dict())
                    parsed_url = urllib.parse.urlparse(url)
                    ssi = urllib.parse.parse_qs(parsed_url.query)['ssi'][0]
                    return ssi
                else:
                    return None
        except requests.exceptions.RequestException as e:
            print("failed to check now, error is {}".format(e))
            return None

    # 绑定账号
    # 返回参数 pltn
    def bind_account(self, account_info, x_aos_stk, ssi):
        try:
            url = self.appstore_host + '/shop/signIn/idms/authx?ssi={}&up=true'.format(ssi)

            headers = self.get_common_request_header()
            headers['Cookie'] += ';myacinfo={}'.format(account_info)
            headers['Content-Type'] = 'application/x-www-form-urlencoded'
            headers['X-Aos-Stk'] = x_aos_stk
            headers['X-Requested-With'] = 'Fetch'
            headers['X-Aos-Model-Page'] = 'signInPage'
            body = 'deviceID=TF1%3B015%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3BMozilla%3BNetscape%3B5.0%2520%2528Windows%2520NT%252010.0%253B%2520Win64%253B%2520x64%2529%2520AppleWebKit%2F537.36%2520%2528KHTML%252C%2520like%2520Gecko%2529%2520Chrome%2F122.0.0.0%2520Safari%2F537.36%3B20030107%3Bundefined%3Btrue%3B%3Btrue%3BWin32%3Bundefined%3BMozilla%2F5.0%2520%2528Windows%2520NT%252010.0%253B%2520Win64%253B%2520x64%2529%2520AppleWebKit%2F537.36%2520%2528KHTML%252C%2520like%2520Gecko%2529%2520Chrome%2F122.0.0.0%2520Safari%2F537.36%3Bzh-CN%3Bundefined%3Bsecure6.store.apple.com%3Bundefined%3Bundefined%3Bundefined%3Bundefined%3Bfalse%3Bfalse%3B1719370448096%3B8%3B2005%2F6%2F7%252021%253A33%253A44%3B1366%3B768%3B%3B%3B%3B%3B%3B%3B%3B-480%3B-480%3B2024%2F6%2F26%252010%253A54%253A08%3B24%3B1366%3B728%3B0%3B0%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B%3B25%3B&grantCode='
            response = requests.post(url, headers=headers, data=body, proxies=self.proxies)
            if not response.ok:
                print("failed to bind account, error is {}".format(response))
                return None

            self.cookies.update(response.cookies.get_dict())
            data = response.content.decode('utf-8')
            root = json.loads(data)
            pltn = root['head']['data']['args']['pltn']
            return pltn
        except requests.exceptions.RequestException as e:
            print("failed to bind account, error is {}".format(e))
            return None

    # 开始下订单
    def checkout_start(self, pltn):
        try:
            url = self.appstore_host + '/shop/checkout/start'
            headers = self.get_common_request_header()
            headers['Content-Type'] = 'application/x-www-form-urlencoded'
            body = 'pltn={}'.format(pltn)
            response = requests.post(url, headers=headers, data=body, proxies=self.proxies)
            if not response.ok:
                print("failed to checkout start, error is {}".format(response))
                return False

            self.cookies.update(response.cookies.get_dict())
            return True
        except requests.exceptions.RequestException as e:
            print("failed to checkout start, error is {}".format(e))
            return False

    def billing(self, x_aos_stk, data_model):
        try:
            url = self.appstore_host + '/shop/checkoutx/billing?_a=continueFromBillingToUnknown&_m=checkout.billing'
            headers = self.get_common_request_header()
            headers['Content-Type'] = 'application/x-www-form-urlencoded'
            headers['X-Aos-Stk'] = x_aos_stk
            headers['X-Requested-With'] = 'Fetch'
            headers['X-Aos-Model-Page'] = 'checkoutPage'

            card_number_for_detection = data_model.credit_card_no[0:4] + ' ' + data_model.credit_card_no[4:6]
            public_key = 'MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAvUIrYPRsCjQNCEGNWmSp9Wz+5uSqK6nkwiBq254Q5taDOqZz0YGL3s1DnJPuBU+e8Dexm6GKW1kWxptTRtva5Eds8VhlAgph8RqIoKmOpb3uJOhSzBpkU28uWyi87VIMM2laXTsSGTpGjSdYjCbcYvMtFdvAycfuEuNn05bDZvUQEa+j9t4S0b2iH7/8LxLos/8qMomJfwuPwVRkE5s5G55FeBQDt/KQIEDvlg1N8omoAjKdfWtmOCK64XZANTG2TMnar/iXyegPwj05m443AYz8x5Uw/rHBqnpiQ4xg97Ewox+SidebmxGowKfQT3+McmnLYu/JURNlYYRy2lYiMwIDAQAB'
            cipher_text = AppleUtil.encrypt(public_key, data_model.credit_card_no.encode('utf-8'))
            card_number = '{"cipherText":"%s==","publicKeyHash":"DsCuZg+6iOaJUKt5gJMdb6rYEz9BgEsdtEXjVc77oAs=="}' % cipher_text
            body = {
                "checkout.billing.billingOptions.selectBillingOption": 'CREDIT',
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.city': data_model.city,
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.state': data_model.state,
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.lastName': data_model.last_name,
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.firstName': data_model.first_name,
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.countryCode': 'JP',
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.street': data_model.street,
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.postalCode': data_model.postal_code,
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.billingAddress.address.street2': data_model.street2,
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.validCardNumber': 'true',
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.cardNumberForBinDetection': card_number_for_detection,
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.selectCardType': 'MASTERCARD',
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.securityCode': data_model.cvv,
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.expiration': data_model.expired_date,
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.cardInputs.cardInput-0.cardNumber': card_number,
                'checkout.billing.billingOptions.selectedBillingOptions.creditCard.creditInstallment.selectedInstallment': '1',
                'checkout.billing.billingOptions.selectedBillingOptions.giftCard.giftCardInput.deviceID': '',
                'checkout.billing.billingOptions.selectedBillingOptions.giftCard.giftCardInput.giftCard': ''
            }
            body = '&'.join(f'{urllib.parse.quote(k, encoding="utf-8", safe="")}={urllib.parse.quote(v, encoding="utf-8", safe="")}' for k, v in body.items())

            response = requests.post(url, headers=headers, data=body, proxies=self.proxies)
            if not response.ok:
                print("failed to create order, error is {}".format(response))
                return False
            else:
                self.cookies.update(response.cookies.get_dict())
                return True
        except requests.exceptions.RequestException as e:
            print("failed to create order, error is {}".format(e))
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
    # apple_util.login('ii5pfw9mvfw9i@163.com', 'Qf223322')
    apple_util.login('suofeibuzi_992@163.com', 'Qf223322')


def test_add_cart():
    apple_util = AppleUtil()
    apple_util.add_cart('iphone-15-pro', 'MU713J')
    apple_util.open_cart()


# 测试全流程
def test():
    apple_util = AppleUtil()

    data_model = DataModel()
    data_model.account = 'suofeibuzi_992@163.com'
    data_model.password = 'Qf223322'
    data_model.first_name = 'Ye'
    data_model.last_name = 'Jeric'
    data_model.telephone = '08054897787'
    data_model.email = '415571971@qq.com'
    data_model.credit_card_no = '5353074484065468'
    data_model.expired_date = '03/26'
    data_model.cvv = '165'
    data_model.postal_code = '104-8125'
    data_model.state = '福建'
    data_model.city = 'Fuzhou'
    data_model.street = 'Changshan'
    data_model.street2 = 'Zhaixianyuan'
    data_model.giftcard_no = ''

    need_comment = False
    if not need_comment:
        # 登录
        account_info = apple_util.login(data_model.account, data_model.password)
        if account_info is None:
            return
        print('account info')
        print(account_info)

        # 添加商品
        model = 'iphone-15-pro'
        product = 'MTU93J'
        if not apple_util.add_cart(model, product):
            return

        cookies = '; '.join(f'{key}={value}' for key, value in apple_util.cookies.items())
        print('cookie after add cart')
        print(cookies)

        # 查询是否有货
        if not apple_util.query_product_available(data_model.postal_code, product):
            return

        # 打开购物车
        x_aos_stk = apple_util.open_cart()
        if x_aos_stk is None:
            return
        print('x_aos_stk={}'.format(x_aos_stk))
    else:
        account_info = ''
        apple_util.debug_cookie = 'dslang=US-EN; site=USA; aasp=6E931A022EF4AA386840B4525C00403A2CE8778FC4585C868A3E1FC505AA16F05AE6E184A9BB0CECB207CBE3ADADBADEADFC5F60D6395A3BAA76C261A104FDD0319A06CF9A2B477C048E2751EEF20A64ADEDFC65287DE98B7A7DBD390DDA2B7F71853BA6EAFE921A26B42D650CF0E3BCBE27C8DADF09486C; myacinfo=DAWTKNV323952cf8084a204fb20ab2508441a07d02d39bd90a1a4c7e1158160ba12f9815dda262b6aa477054e016d4dd8be8edc63e0437265a42663e455e3467221b17117236065927a38667f01440b276dc95d8777685a55f8f9da83cbfdb61a0e4c918ecd63738d456c9bd9df768ebf63d77cc18129588ea317688a22ad38c24f67686a748f552bf5f0627d37417a2397116a239cd221f8fb78c5749aed06b3828d13a66bb32af2bd61477434d37efa02083d628eaa63f6e85fd61f4bfd83a6b4001c893e00f1bc24855088121bc9951a7afa7e381495f33293c4a18899991b627604906d9736c4bcb2ce22b313105ccd204a64bef9443c88ac333b885fdd8306dc21b2afb101da26fbb93a6a22a400ecf81aebd85fce61cb8c90c9d08bdd872abf07bd2a07942b47c3e34d3addd9585f7809c0e44917ab2ba834741cb79ce98aff42de8f2f5a39c8785840220b5b09452ebf9533bde483f3bff1e128c192fd7f51b82050e8c8eb33dee3f3009e892e3f70a53326bad76ddb91a637c56d1941affc734cc68471ce7e5efb0b83c25c8a9893ec7b178b44a82b26f3ff36df8048f34ebcd69ce9003e652b7e7c217448dd7baf3b3d375b88594c1ea99b1b1e0f278fcf5a2b68cea3c0cc208c3a7e5ff7a531088bc07b81db8355870aa6e9fb0642cfb7d9173d8bcf32c0c6e2d361d8a73519c0e27e1c2585a47V3; as_dc=ucp4; as_pcts=jBLenuBb+BOcncSC7n_Q7i2vgEhlyKDC-xGE7+JDdWVZ4OmuI7A-paO7MfFiAdgUubXQ0VhdvPoDHIyH-cl1+6giv8X_Z8tFLl+hgl-gG6robDYxCxjRqM-q9FhJG_eSM3jgmqPfq3W34JR5qfWcuuQjUFPSkqHt7bHpQp0c; dssf=1; dssid2=d3a2b610-70bc-4bc3-8b89-06f40569c6d4; as_atb=1.0|MjAyNC0wNi0yNiAwNzozNTo1Mw|f08a746eb63ad6f9eaa5089ce87a39612cd3531d'
        x_aos_stk = 'X6VBv3b56_Kp-YuFZr8QPHSfFgqei1kx6QjXSdeFtl4'

    # 进入购物流程
    ssi = apple_util.check_now(x_aos_stk)
    if ssi is None:
        return

    # 绑定账号
    pltn = apple_util.bind_account(account_info, x_aos_stk, ssi)
    if pltn is None:
        return

    # 开始下单
    if not apple_util.checkout_start(pltn):
        return

    # 支付
    if not apple_util.billing(x_aos_stk, data_model):
        return


if __name__ == '__main__':
    # test_encrypt()
    # test_login()
    # test_add_cart()
    # test_query_product_available()
    # test_login_and_addcart()
    # test_order()
    test()
    print('done')

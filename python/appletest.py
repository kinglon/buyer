from datamodel import DataModel
from apple import AppleUtil
from logutil import LogUtil
import time


def test_encrypt():
    public_key = 'MIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAvUIrYPRsCjQNCEGNWmSp9Wz+5uSqK6nkwiBq254Q5taDOqZz0YGL3s1DnJPuBU+e8Dexm6GKW1kWxptTRtva5Eds8VhlAgph8RqIoKmOpb3uJOhSzBpkU28uWyi87VIMM2laXTsSGTpGjSdYjCbcYvMtFdvAycfuEuNn05bDZvUQEa+j9t4S0b2iH7/8LxLos/8qMomJfwuPwVRkE5s5G55FeBQDt/KQIEDvlg1N8omoAjKdfWtmOCK64XZANTG2TMnar/iXyegPwj05m443AYz8x5Uw/rHBqnpiQ4xg97Ewox+SidebmxGowKfQT3+McmnLYu/JURNlYYRy2lYiMwIDAQAB'
    encrypt_data = AppleUtil.encrypt(public_key, '5353079623725781'.encode('utf-8'))
    print(encrypt_data)


def test_get_complete_data():
    apple_util = AppleUtil()
    account_name = 'ii5pfw9mvfw9i@163.com'
    password = 'Qf223322'
    salt = 'SInpSF5/lsMlDU5xPZIa8w=='
    iteration = 20173
    server_public_value = "qjdprOBfPulmYIVCSufUJ0/FG4Tc1zvCU8FuyP4WLMDu2eD4YVuPa8leIyqXZGPYuRKmbDwXDHetmOgifrCfz/aS8uXGQSJdEbxjqp3oE4gGaD3UW0XXx51JMFe2ZrQ+eKMsnKXVDHDh9UwY3MwcpVZ2wRZav24HiwvCYifmqaFZSu15maMa+jFScI1kfxLIs2HX5l70U1oCJ9Yk20oNXdXvdQxN+wmHVoCy95mt2GHBa/l17MoZD0dtf3QywJlmcasm4Tlz1wgQM2txROZOP0tCm6wusiYUmjoEUzIWAlgDDBEY5EM/T1/jnUdK8YplH/TN2WJVmt8+A21jX0iySw=="
    private_value = bytes(
        [187, 190, 210, 164, 71, 19, 67, 236, 35, 91, 123, 152, 211, 101, 163, 141, 128, 113, 150, 105, 113, 83, 114, 184, 16, 202, 55, 63, 109, 166, 144, 159, 46, 192, 243, 182, 135, 207, 64, 206, 163, 12, 168, 249, 173, 235, 159, 102, 109, 177, 225, 110, 49, 144, 74, 99, 250, 71, 185, 150, 35, 200, 104, 151, 115, 51, 61, 161, 77, 53, 177, 157, 4, 206, 138, 224, 223, 72, 201, 175, 247, 205, 79, 39, 199, 244, 219, 203, 169, 68, 83, 124, 39, 78, 226, 35, 23, 214, 134, 241, 146, 53, 115, 191, 221, 14, 24, 68, 177, 52, 17, 94, 249, 86, 143, 162, 4, 218, 118, 41, 207, 225, 2, 91, 110, 122, 231, 255, 82, 75, 175, 218, 193, 244, 21, 52, 138, 152, 34, 97, 188, 41, 169, 203, 82, 103, 228, 142, 170, 245, 87, 244, 62, 169, 217, 206, 163, 211, 220, 21, 180, 34, 135, 255, 201, 119, 43, 225, 177, 79, 161, 159, 31, 147, 113, 48, 55, 226, 8, 112, 252, 70, 251, 29, 236, 75, 58, 85, 241, 205, 137, 151, 72, 166, 255, 134, 35, 99, 116, 141, 237, 17, 3, 89, 83, 46, 70, 76, 85, 246, 223, 53, 205, 156, 45, 152, 62, 176, 62, 110, 156, 128, 116, 246, 143, 31, 241, 167, 255, 174, 126, 168, 132, 97, 238, 162, 183, 202, 11, 201, 223, 186, 59, 190, 49, 149, 171, 145, 196, 162, 13, 122, 10, 77, 81, 47])
    public_value = bytes(
        [67, 15, 167, 201, 151, 223, 87, 78, 170, 92, 245, 179, 204, 44, 8, 109, 78, 26, 167, 125, 185, 136, 185, 102, 211, 100, 166, 160, 222, 244, 181, 78, 250, 80, 114, 98, 83, 137, 69, 191, 224, 166, 36, 119, 14, 79, 65, 82, 46, 12, 69, 53, 154, 152, 144, 171, 186, 235, 180, 41, 237, 99, 8, 222, 23, 23, 191, 253, 11, 241, 2, 74, 121, 131, 112, 216, 20, 223, 219, 236, 253, 120, 255, 180, 85, 159, 61, 123, 106, 217, 201, 191, 54, 246, 234, 135, 61, 244, 16, 221, 213, 221, 180, 196, 48, 39, 216, 108, 32, 138, 252, 72, 181, 168, 187, 148, 178, 216, 139, 114, 83, 76, 68, 179, 153, 55, 235, 162, 13, 179, 77, 38, 254, 49, 120, 105, 193, 153, 90, 115, 150, 253, 102, 84, 53, 226, 126, 242, 134, 41, 138, 207, 130, 5, 114, 67, 164, 28, 76, 99, 194, 65, 132, 153, 50, 38, 252, 192, 138, 228, 198, 195, 33, 113, 188, 118, 109, 5, 194, 11, 219, 59, 106, 206, 137, 28, 148, 24, 29, 178, 160, 161, 125, 18, 43, 73, 4, 184, 47, 86, 122, 13, 130, 215, 150, 237, 158, 187, 153, 156, 66, 33, 194, 71, 131, 153, 130, 90, 55, 123, 200, 255, 125, 109, 105, 14, 237, 124, 76, 2, 158, 0, 8, 108, 166, 218, 192, 73, 90, 191, 180, 153, 206, 201, 108, 44, 135, 160, 108, 240, 144, 34, 226, 14, 136, 16])

    apple_util.get_complete_data(account_name, password, salt, iteration, private_value, public_value,
                                 server_public_value)


def test_login():
    apple_util = AppleUtil()
    # apple_util.login('ii5pfw9mvfw9i@163.com', 'Qf223322')
    apple_util.login('suofeibuzi_992@163.com', 'Qf223322')


def test_add_cart():
    apple_util = AppleUtil()
    apple_util.add_cart('iphone-15-pro', 'MU713J')
    apple_util.open_cart()


# 测试全流程V2，优化步骤
def test_v2():
    apple_util = AppleUtil()

    data_model = DataModel()
    data_model.account = 'm16670802@163.com'
    data_model.password = 'Cxh520941'
    data_model.store = 'R079'
    data_model.store_postal_code = '104-0061'
    data_model.first_name = 'Jeric'
    data_model.last_name = 'Ye'
    data_model.telephone = '08054897787'
    data_model.email = '415571971@qq.com'
    data_model.credit_card_no = '5478825275225078'
    data_model.expired_date = '03/25'
    data_model.cvv = '026'
    data_model.postal_code = '104-8125'
    data_model.state = '北海道'
    data_model.city = 'Fuzhou'
    data_model.street = 'Changshan'
    data_model.street2 = 'Zhaixianyuan'
    data_model.giftcard_no = ''

    # 添加商品
    model = 'iphone-15'
    product = 'MTML3J'
    # model = 'apple-watch-ultra'
    # product = 'MX4F3J'
    # model = 'iphone-16'
    # product = 'MYDU3J'
    if not apple_util.add_cart(model, product):
        return

    # 打开购物车
    x_aos_stk = apple_util.open_cart()
    if x_aos_stk is None:
        return

    # 进入购物流程
    ssi = apple_util.checkout_now(x_aos_stk)
    if ssi is None:
        return

    # 登录
    if not apple_util.login(data_model.account, data_model.password):
        return

    # 绑定账号
    pltn = apple_util.bind_account(x_aos_stk, ssi)
    if pltn is None:
        return

    # 开始下单
    if not apple_util.checkout_start(pltn):
        return

    # 进入下单页面
    x_aos_stk = apple_util.checkout()
    if x_aos_stk is None:
        return

    # 选择自提
    if not apple_util.fulfillment_retail(x_aos_stk):
        return

    # 查询是否有货
    if not apple_util.query_product_available(data_model.store_postal_code, product):
        return

    # 查询可购买日期和时间
    success, pickup_date, pickup_time = apple_util.fulfillment_pickup_datetime(x_aos_stk, data_model)
    if not success:
        return

    # 选择店铺
    if not apple_util.fulfillment_store(x_aos_stk, data_model, pickup_date, pickup_time):
        return

    # 选择联系人
    if not apple_util.pickup_contact(x_aos_stk, data_model):
        return

    # 输入账单，支付
    if not apple_util.billing(x_aos_stk, data_model):
        return

    # 确认下单
    if not apple_util.review(x_aos_stk):
        return

    # 查询处理结果
    while True:
        time.sleep(2)
        # 处理订单
        finish, sucess = apple_util.query_process_result(x_aos_stk)
        if not finish:
            continue
        break

    if not sucess:
        return

    # 查询订单号
    order_no = apple_util.query_order_no()
    if order_no is None:
        print('未查询到订单号')
        return
    print('订单号是：{}'.format(order_no))


def main():
    # test_encrypt()
    # test_get_complete_data()
    # test_login()
    # test_add_cart()
    # test_query_product_available()
    # test_login_and_addcart()
    # test_order()
    # test()
    test_v2()
    print('done')


if __name__ == '__main__':
    LogUtil.enable()
    main()

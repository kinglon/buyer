import os.path
import sys
import time
import traceback
from logutil import LogUtil
from apple import AppleUtil
import requests
import json
from stateutil import StateUtil
import threading
from setting import Setting
from datetime import datetime
from datamodel import DataModel

# 工作目录
g_work_path = ''

# 购买结果列表
g_buy_results = []

# 同步锁
g_lock = threading.Lock()


class BuyResult:
    def __int__(self):
        # 购买信息
        self.buy_info = None

        # 标志是否成功下单
        self.success = False

        # 订单号
        self.order_number = ''

        # 下单失败原因
        self.fail_reason = ''

        # 开始购买的时间，utc秒数
        self.begin_buy_time = 0

        # 上号的代理地址
        self.proxy_server = ''

        # 购买店铺名字
        self.buy_shop_name = ''


# 获取代理IP列表，返回（success, proxy ips, message）
def get_proxy_server_list(proxy_region):
    no_proxy_server = {"http": "", "https": ""}
    error = (False, [], '')
    try:
        url = ("http://api.proxy.ipidea.io/getBalanceProxyIp?big_num=900&return_type=json&lb=1&sb=0&flow=1&regions={}&protocol=socks5"
               .format(proxy_region))
        response = requests.get(url, proxies=no_proxy_server, timeout=10)
        if not response.ok:
            print("获取代理IP列表失败，错误是：{}".format(response))
            return error
        else:
            data = response.content.decode('utf-8')
            data = json.loads(data)
            if data['success']:
                return True, data['data'], ''
            else:
                return True, [], data['msg']
    except Exception as e:
        print("获取代理IP列表失败，错误是：{}".format(e))
        return error


def buy(proxys, datamodel):
    for i in range(len(users)):
        account = users[i]['account']
        password = users[i]['password']

        success = False
        error_message = ''
        for j in range(3):
            print('开始上号，账号是：{}'.format(account))
            apple_util = AppleUtil()

            # 设置代理IP
            proxy_ip = ''
            proxy_port = 0
            if len(proxys) > 0:
                proxy_server = proxys[i % len(proxys)]
                proxy_ip = proxy_server['ip']
                proxy_port = proxy_server['port']
                proxy_address = "socks5://{}:{}".format(proxy_ip, proxy_port)
                proxy = {"http": proxy_address, "https": proxy_address}
            else:
                proxy = {"http": "", "https": ""}
            apple_util.proxies = proxy
            print('使用代理IP：{}'.format(proxy))

            # 添加商品
            model = phone_model['model']
            code = phone_model['phone_code']
            print('加入购物包: {}, {}'.format(model, code))
            if not apple_util.add_cart(model, code):
                error_message = '加入购物包失败: {}, {}'.format(model, code)
                continue
            time.sleep(Setting.get().request_interval)

            # 打开购物车
            print('打开购物包')
            x_aos_stk = apple_util.open_cart()
            if x_aos_stk is None:
                error_message = '打开购物包失败'
                continue
            time.sleep(Setting.get().request_interval)

            # 进入购物流程
            print('进入购物流程')
            ssi = apple_util.checkout_now(x_aos_stk)
            if ssi is None:
                error_message = '进入购物流程失败'
                continue
            time.sleep(Setting.get().request_interval)

            # 登录
            print('登录账号')
            if not apple_util.login(account, password):
                error_message = '登录账号失败：{}, {}'.format(account, password)
                continue
            time.sleep(Setting.get().request_interval)

            # 绑定账号
            print('绑定账号')
            pltn = apple_util.bind_account(x_aos_stk, ssi)
            if pltn is None:
                error_message = '绑定账号失败'
                continue
            time.sleep(Setting.get().request_interval)

            # 开始下单
            print('开始下单')
            if not apple_util.checkout_start(pltn):
                error_message = '开始下单失败'
                continue
            time.sleep(Setting.get().request_interval)

            # 进入下单页面
            print('进入下单页面')
            x_aos_stk = apple_util.checkout()
            if x_aos_stk is None:
                error_message = '进入下单页面失败'
                continue
            time.sleep(Setting.get().request_interval)

            # 选择自提
            print('选择自提')
            if not apple_util.fulfillment_retail(x_aos_stk):
                error_message = '选择自提失败'
                continue
            time.sleep(Setting.get().request_interval)

            buy_param = {
                'account': account,
                'appstore_host': apple_util.appstore_host,
                'x_aos_stk': x_aos_stk,
                'proxy_ip': '',
                "proxy_port": 0,
                'cookies': apple_util.cookies
            }
            if len(proxy_ip) > 0:
                buy_param['proxy_ip'] = proxy_ip
                buy_param['proxy_port'] = proxy_port
            StateUtil.get().finish_task(True, buy_param, '')
            success = True
            break
        if not success:
            message = '上号失败，账号是：{}，原因是：{}'.format(account, error_message)
            StateUtil.get().finish_task(False, None, message)


# 加载数据，返回(success, buy_time, [datamodel])
def load_data():
    failed_result = (False, 0, [])
    try:
        param_file_path = os.path.join(g_work_path, 'python_args_buy.json')
        with open(param_file_path, 'r', encoding='utf-8') as file:
            json_data = file.read()
            root = json.loads(json_data)
            plan_id = root['plan_id']
            users = root['user']

        current_file_path = os.path.dirname(os.path.abspath(__file__))
        plan_file_path = os.path.join(current_file_path, '..', 'Configs', 'plan.json')
        with open(plan_file_path, 'r', encoding='utf-8') as file:
            json_data = file.read()
            root = json.loads(json_data)
            plans = root['plan']

        current_plan = None
        for plan in plans:
            if plan_id == plan['id']:
                current_plan = plan
                break
        if current_plan is None:
            print('找不到计划')
            return failed_result
        if not current_plan['enable_fix_time_buy']:
            print('不是设定时间开始购买')
            return failed_result

        config_file_path = os.path.join(current_file_path, '..', 'Configs', 'configs.json')
        with open(config_file_path, 'r', encoding='utf-8') as file:
            json_data = file.read()
            root = json.loads(json_data)
            shops = root['shop']

        current_shops = []
        for plan_shop in current_plan['shop']:
            found = False
            for shop in shops:
                if shop['name'] == plan_shop["name"]:
                    found = True
                    current_shops.append(shop)
                    break
            if not found:
                print("无法找到店铺")
                return failed_result
        if len(current_shops) == 0:
            print('没有配置店铺')
            return failed_result

        data_models = []
        next_shop = 0
        for user in users:
            data_model = DataModel()
            data_model.account = user['account']
            data_model.state = user['state']
            data_model.password = user['password']
            data_model.city = user['city']
            data_model.email = user['email']
            data_model.first_name = user['first_name']
            data_model.postal_code = user['postal_code']
            data_model.store = current_shops[next_shop]['id']
            data_model.store_name = current_shops[next_shop]['name']
            data_model.store_postal_code = current_shops[next_shop]['postal']
            data_model.street2 = user['street2']
            data_model.street = user['street']
            data_model.telephone = user['telephone']
            data_model.last_name = user['last_name']
            data_model.expired_date = user['expired_date']
            data_model.credit_card_no = user['credit_card_no']
            data_model.cvv = user['cvv']
            data_model.giftcard_no = ''
            data_models.append(data_model)
            next_shop = (next_shop + 1) % len(current_shops)
        return True, current_plan['fix_buy_time'], data_models
    except Exception as e:
        print('加载数据遇到问题：{}'.format(e))
        return failed_result


# 保存购买结果到表格文件
def save_buy_result_to_excel():
    pass


# 保存购买结果到表格文件
def save_buy_result_to_json():
    pass


def main():
    global g_work_path
    g_work_path = sys.argv[1]

    # 启用日志
    LogUtil.set_log_path(g_work_path)
    LogUtil.enable()

    print('加载数据')
    success, buy_time, datamodels = load_data()
    if not success:
        return

    # 获取代理IP列表
    proxy_server_list = []
    message = ''
    for i in range(3):
        print('获取代理IP列表')
        success, proxy_server_list, message = get_proxy_server_list('jp')
        if not success:
            continue
        break

    if len(proxy_server_list) == 0:
        print('获取代理IP列表失败，错误是：{}'.format(message))
        return

    # 倒计时
    last_print_time = 0
    update_proxy = False
    while True:
        time.sleep(0.01)
        now = datetime.now().timestamp()
        if now >= buy_time:
            break
        elapse = buy_time - now
        if now - last_print_time >= 10:  # 每隔10秒打印一次
            print('倒计时：{}秒'.format(elapse))
            last_print_time = now

        # 最后5分钟更新下代理
        if not update_proxy and elapse < 300:
            update_proxy = True
            success, proxy_server_list2, message = get_proxy_server_list('jp')
            if success:
                proxy_server_list = proxy_server_list2

    # 多线程并发购买
    threads = []
    thread_num = len(datamodels)
    proxy_count_per_thread = len(proxy_server_list) // thread_num
    for datamodel in datamodels:
        if i < thread_num - 1:
            proxys = proxy_server_list[i * proxy_count_per_thread: (i + 1) * proxy_count_per_thread]
        else:
            proxys = proxy_server_list[i * proxy_count_per_thread:]
        args = (proxys, datamodel)
        thread = threading.Thread(target=buy, args=args)
        thread.start()
        threads.append(thread)
    for thread in threads:
        thread.join()

    # 保存购买结果到表格文件
    save_buy_result_to_excel()


if __name__ == '__main__':
    main()
    print('购买完成')

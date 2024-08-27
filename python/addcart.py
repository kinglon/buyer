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


# 获取代理IP列表，返回（success, proxy ips, message）
def get_proxy_server_list(proxy_region):
    no_proxy_server = {"http": "", "https": ""}
    error = (False, [], '')
    try:
        url = ("http://api.proxy.ipidea.io/getBalanceProxyIp?num=900&return_type=json&lb=1&sb=0&flow=1&regions={}&protocol=socks5"
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


def add_cart(proxys, users, phone_model):
    for i in range(len(users)):
        account = users[i]['account']
        password = users[i]['password']

        success = False
        error_message = ''
        for j in range(3):
            print('开始上号，账号是：{}'.format(account))
            apple_util = AppleUtil()

            # 设置代理IP
            if len(proxys) > 0:
                proxy_server = proxys[i % len(proxys)]
                proxy_address = "socks5://{}:{}".format(proxy_server['ip'], proxy_server['port'])
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
                'cookies': apple_util.cookies
            }
            StateUtil.get().finish_task(True, buy_param, '')
            success = True
            break
        if not success:
            message = '上号失败，账号是：{}，原因是：{}'.format(account, error_message)
            StateUtil.get().finish_task(False, None, message)


def main():
    work_path = sys.argv[1]

    # 启用日志
    LogUtil.set_log_path(work_path)
    LogUtil.enable()

    StateUtil.get().saved_path = work_path

    print('开始上号')

    try:
        # 加载参数
        print("加载参数")
        param_file_path = os.path.join(work_path, 'python_args.json')
        with open(param_file_path, 'r', encoding='utf-8') as file:
            json_data = file.read()
            root = json.loads(json_data)
            use_proxy = root['use_proxy']
            thread_num = root['thread_num']
            phone_model = root['phone_model']
            proxy_region = root['proxy_region']
            users = root['user']
        if len(users) == 0:
            StateUtil.get().set_finish('没有任何账号')
            return
        StateUtil.get().total = len(users)

        # 获取代理IP列表
        proxy_server_list = []
        if use_proxy:
            message = ''
            for i in range(3):
                print('获取代理IP列表')
                success, proxy_server_list, message = get_proxy_server_list(proxy_region)
                if not success:
                    continue
                break

            if len(proxy_server_list) == 0:
                message = '获取代理IP列表失败，错误是：{}'.format(message)
                print(message)
                StateUtil.get().set_finish(message)
                return

        # 多线程并发
        threads = []
        user_count_per_thread = len(users) // thread_num
        proxy_count_per_thread = len(proxy_server_list) // thread_num
        for i in range(thread_num):
            if i < thread_num - 1:
                user = users[i * user_count_per_thread: (i + 1) * user_count_per_thread]
                proxy = proxy_server_list[i * proxy_count_per_thread: (i + 1) * proxy_count_per_thread]
            else:
                user = users[i * user_count_per_thread:]
                proxy = proxy_server_list[i * proxy_count_per_thread:]
            args = (proxy, user, phone_model)
            thread = threading.Thread(target=add_cart, args=args)
            thread.start()
            threads.append(thread)
        for thread in threads:
            thread.join()
        StateUtil.get().set_finish('')
    except Exception as e:
        print("遇到问题：{}".format(e))
        traceback.print_exc()
        StateUtil.get().set_finish('遇到未知问题')


if __name__ == '__main__':
    main()
    print('上号完成')

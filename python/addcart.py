import os.path
import sys
import time
import traceback

from requests_toolbelt.adapters import source

from logutil import LogUtil
from apple import AppleUtil
import requests
import json
from stateutil import StateUtil
import threading
from setting import Setting
import socket


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


def add_cart(proxys, users, local_ips, phone_model):
    for i in range(len(users)):
        account = users[i]['account']
        password = users[i]['password']

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

        fail_reason_prefix = '上号失败，账号是：{}，原因是：'.format(account)

        # 使用本地IP地址
        local_ip = local_ips[i % len(local_ips)]
        try:
            print('使用本地IP：{}'.format(local_ip))
            session = requests.Session()
            new_source = source.SourceAddressAdapter(local_ip)
            session.mount('http://', new_source)
            session.mount('https://', new_source)
            apple_util.session = session
        except Exception as e:
            print('遇到问题：{}'.format(e))
            error_message = '指定本地IP({})发送失败'.format(local_ip)
            StateUtil.get().finish_task(False, None, fail_reason_prefix + error_message)
            continue

        max_retry_count = 3

        # 添加商品
        model = phone_model['model']
        code = phone_model['phone_code']
        error_message = ''
        for j in range(max_retry_count):
            print('加入购物包: {}, {}'.format(model, code))
            if not apple_util.add_cart(model, code):
                error_message = '加入购物包失败: {}, {}'.format(model, code)
                continue
            break
        if len(error_message) > 0:
            StateUtil.get().finish_task(False, None, fail_reason_prefix + error_message)
            continue
        time.sleep(Setting.get().request_interval)

        # 打开购物车
        error_message = ''
        x_aos_stk = ''
        for j in range(max_retry_count):
            print('打开购物包')
            x_aos_stk = apple_util.open_cart()
            if x_aos_stk is None:
                error_message = '打开购物包失败'
                continue
            break
        if len(error_message) > 0:
            StateUtil.get().finish_task(False, None, fail_reason_prefix + error_message)
            continue
        time.sleep(Setting.get().request_interval)

        # 进入购物流程
        error_message = ''
        ssi = ''
        for j in range(max_retry_count):
            print('进入购物流程')
            ssi = apple_util.checkout_now(x_aos_stk)
            if ssi is None:
                error_message = '进入购物流程失败'
                continue
            break
        if len(error_message) > 0:
            StateUtil.get().finish_task(False, None, fail_reason_prefix + error_message)
            continue
        time.sleep(Setting.get().request_interval)

        # 登录
        error_message = ''
        for j in range(max_retry_count):
            print('登录账号')
            if not apple_util.login(account, password):
                error_message = '登录账号失败：{}, {}'.format(account, password)
                continue
            break
        if len(error_message) > 0:
            StateUtil.get().finish_task(False, None, fail_reason_prefix + error_message)
            continue
        time.sleep(Setting.get().request_interval)

        # 绑定账号
        error_message = ''
        pltn = ''
        for j in range(max_retry_count):
            print('绑定账号')
            pltn = apple_util.bind_account(x_aos_stk, ssi)
            if pltn is None:
                error_message = '绑定账号失败'
                continue
            break
        if len(error_message) > 0:
            StateUtil.get().finish_task(False, None, fail_reason_prefix + error_message)
            continue
        time.sleep(Setting.get().request_interval)

        # 开始下单
        error_message = ''
        for j in range(max_retry_count):
            print('开始下单')
            if not apple_util.checkout_start(pltn):
                error_message = '开始下单失败'
                continue
            break
        if len(error_message) > 0:
            StateUtil.get().finish_task(False, None, fail_reason_prefix + error_message)
            continue
        time.sleep(Setting.get().request_interval)

        # 进入下单页面
        error_message = ''
        for j in range(max_retry_count):
            print('进入下单页面')
            x_aos_stk = apple_util.checkout()
            if x_aos_stk is None:
                error_message = '进入下单页面失败'
                continue
            break
        if len(error_message) > 0:
            StateUtil.get().finish_task(False, None, fail_reason_prefix + error_message)
            continue
        time.sleep(Setting.get().request_interval)

        # 选择自提
        error_message = ''
        for j in range(max_retry_count):
            print('选择自提')
            if not apple_util.fulfillment_retail(x_aos_stk):
                error_message = '选择自提失败'
                continue
            break
        if len(error_message) > 0:
            StateUtil.get().finish_task(False, None, fail_reason_prefix + error_message)
            continue

        buy_param = {
            'account': account,
            'appstore_host': apple_util.appstore_host,
            'x_aos_stk': x_aos_stk,
            'proxy_ip': '',
            "proxy_port": 0,
            "local_ip": local_ip,
            'cookies': apple_util.cookies
        }
        if len(proxy_ip) > 0:
            buy_param['proxy_ip'] = proxy_ip
            buy_param['proxy_port'] = proxy_port
        StateUtil.get().finish_task(True, buy_param, '')


def main():
    work_path = sys.argv[1]

    # 启用日志
    LogUtil.set_log_path(work_path)
    LogUtil.enable()

    StateUtil.get().saved_path = work_path

    print('开始')

    try:
        # 获取本地IP列表
        print('获取本地IP列表')
        local_ips = [ip for ip in socket.gethostbyname_ex(socket.gethostname())[2] if not ip.startswith("127.")]
        if len(local_ips) == 0:
            StateUtil.get().set_finish('本地IP个数为0')
            return
        print('本地IP个数是{}'.format(len(local_ips)))

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
        local_ip_count_per_thread = len(local_ips) // thread_num
        for i in range(thread_num):
            if i < thread_num - 1:
                user = users[i * user_count_per_thread: (i + 1) * user_count_per_thread]
                proxy = proxy_server_list[i * proxy_count_per_thread: (i + 1) * proxy_count_per_thread]
                local_ip = local_ips[i * local_ip_count_per_thread: (i + 1) * local_ip_count_per_thread]
            else:
                user = users[i * user_count_per_thread:]
                proxy = proxy_server_list[i * proxy_count_per_thread:]
                local_ip = local_ips[i * proxy_count_per_thread:]
            if len(local_ip) == 0:
                local_ip.append(local_ips[0])
            args = (proxy, user, local_ip, phone_model)
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
    print('完成')

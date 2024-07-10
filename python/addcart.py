import os.path
import sys
import traceback
from logutil import LogUtil
from apple import AppleUtil
import requests
import json

# 输出结果
g_success = False
g_message = ''
g_buy_params = []

no_proxy_server = {"http": "", "https": ""}

def get_proxy_server_list(proxy_region):
    try:
        url = ("http://api.proxy.ipidea.io/getBalanceProxyIp?num=900&return_type=json&lb=1&sb=0&flow=1&regions={}&protocol=socks5"
               .format(proxy_region))
        response = requests.get(url, proxies=no_proxy_server)
        if not response.ok:
            print("failed to get proxy ips, error is {}".format(response))
            return []
        else:
            data = response.content.decode('utf-8')
            data = json.loads(data)
            if data['success']:
                return data['data']
            else:
                global g_message
                g_message = data['msg']
                return []
    except requests.exceptions.RequestException as e:
        print("failed to get proxy ips, error is {}".format(e))
        return []


def load_params(param_file_path):
    use_proxy = False
    phone_model = {}
    users = []
    proxy_region = ''

    try:
        with open(param_file_path, 'r', encoding='utf-8') as file:
            json_data = file.read()
            root = json.loads(json_data)
            use_proxy = root['use_proxy']
            phone_model = root['phone_model']
            proxy_region = root['proxy_region']
            users = root['user']
            return use_proxy, phone_model, users, proxy_region
    except Exception as e:
        print("failed to load param, error is {}".format(e))
        return use_proxy, phone_model, users, proxy_region


def save(success, message, buy_params, save_file_path):
    if not success and len(message) == 0:
        message = "未知错误"
    root = {'success': success, 'message': message, 'buy_param': buy_params}
    with open(save_file_path, "w", encoding='utf-8') as file:
        json.dump(root, file, indent=4)


def add_cart(work_path):
    global g_message
    global g_success
    global g_buy_params

    # 加载参数
    param_file_path = os.path.join(work_path, 'python_args.json')
    use_proxy, phone_model, users, proxy_region = load_params(param_file_path)
    if len(users) == 0:
        g_message = '参数错误'
        return

    # 获取代理IP列表
    print('begin to get proxy server list')
    proxy_server_list = get_proxy_server_list(proxy_region)
    if len(proxy_server_list) == 0:
        if len(g_message) == 0:
            g_message = '获取代理IP失败'
        return
    print('finish to get proxy server list')

    for i in range(len(users)):
        account = users[i]['account']
        password = users[i]['password']

        ok = False
        for j in range(2):
            apple_util = AppleUtil()

            # 设置代理IP
            if use_proxy:
                proxy_server = proxy_server_list[i % len(proxy_server_list)]
                proxy_address = "socks5://{}:{}".format(proxy_server['ip'], proxy_server['port'])
                proxy = {"http": proxy_address, "https": proxy_address}
            else:
                proxy = no_proxy_server
            apple_util.proxies = proxy

            # 登录
            if not apple_util.login(account, password):
                g_message = '登录失败，用户名({})，密码({})'.format(account, password)
                continue

            # 添加商品
            if not apple_util.add_cart(phone_model['model'], phone_model['phone_code']):
                g_message = '加货失败，机型: {}, {}'.format(phone_model['model'], phone_model['phone_code'])
                continue

            # 打开购物车
            x_aos_stk = apple_util.open_cart()
            if x_aos_stk is None:
                g_message = '加货失败，打开购物袋失败'
                continue

            ok = True
            buy_param = {
                'account': account,
                'x_aos_stk': x_aos_stk,
                'cookies': apple_util.cookies
            }

            g_buy_params.append(buy_param)
            break

        if not ok:
            return
    g_success = True


def main():
    work_path = sys.argv[1]

    # 启用日志
    LogUtil.set_log_path(work_path)
    LogUtil.enable()

    print('begin')
    try:
        add_cart(work_path)
    except Exception as e:
        print("has an error: {}".format(e))
        traceback.print_exc()
    save(g_success, g_message, g_buy_params, os.path.join(work_path, 'add_cart_result.json'))    
    print('done')


if __name__ == '__main__':    
    main()

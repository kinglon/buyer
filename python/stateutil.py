import threading
import os
import json


class StateUtil:
    __instance = None

    @staticmethod
    def get():
        if StateUtil.__instance is None:
            StateUtil.__instance = StateUtil()
        return StateUtil.__instance

    def __init__(self):
        # 同步锁
        self.lock = threading.Lock()

        # 数据保存路径
        self.saved_path = ''

        # 是否完成
        self.finish = False

        # 完成附带的消息
        self.finish_message = ''

        # 总共任务数
        self.total = 0

        # 成功任务数
        self.success_count = 0

        # 失败任务数
        self.fail_count = 0

        # 失败列表
        self.fail_details = []

        # 成功购物参数列表
        self.buy_params = []

    # 保存进度
    def _save_progress(self):
        config_file_path = os.path.join(self.saved_path, r'add_cart_progress.json')
        with open(config_file_path, "w", encoding='utf-8') as file:
            root = {
                'finish': self.finish,
                'finish_message': self.finish_message,
                'total': self.total,
                'success_count': self.success_count,
                'fail_count': self.fail_count
            }
            json.dump(root, file, indent=4)

    # 保存结果
    def _save_result(self):
        config_file_path = os.path.join(self.saved_path, r'add_cart_result.json')
        with open(config_file_path, "w", encoding='utf-8') as file:
            root = {
                'buy_param': self.buy_params
            }
            json.dump(root, file, indent=4)

        config_file_path = os.path.join(self.saved_path, r'上号失败账号.txt')
        with open(config_file_path, 'w') as file:
            for line in self.fail_details:
                file.write(f"{line}\n")

    def set_finish(self, finish_message):
        self.lock.acquire()

        self.finish = True
        self.finish_message = finish_message
        self._save_progress()
        self._save_result()

        self.lock.release()

    def finish_task(self, success, buy_params, fail_detail):
        self.lock.acquire()

        if success:
            self.success_count += 1
            self.buy_params.append(buy_params)
        else:
            self.fail_count += 1
            self.fail_details.append(fail_detail)
        self._save_progress()

        self.lock.release()

import json
import os


class Setting:
    __instance = None

    @staticmethod
    def get():
        if Setting.__instance is None:
            Setting.__instance = Setting()
        return Setting.__instance

    def __init__(self):
        self.request_interval = 1
        self.__load()

    def __load(self):
        current_file_path = os.path.dirname(os.path.abspath(__file__))
        config_file_path = os.path.join(current_file_path, r'configs\configs.txt')
        with open(config_file_path, 'r', encoding='utf-8') as file:
            json_data = file.read()
            root = json.loads(json_data)
            self.request_interval = root['上号请求间隔'] / 1000

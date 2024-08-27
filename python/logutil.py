import sys
import os
import datetime
import threading


class PrintStream:
    lock = threading.Lock()

    def __init__(self, streams):
        self.streams = streams

    def write(self, data):
        PrintStream.lock.acquire()

        for stream in self.streams:
            if data == '\n':
                stream.write(data)
            else:
                current_time = datetime.datetime.now()
                formatted_time = current_time.strftime("[%Y-%m-%d %H:%M:%S]")
                thread_id = '[{}]'.format(threading.current_thread().ident)
                stream.write(formatted_time + thread_id + ' ' + data)
            stream.flush()

        PrintStream.lock.release()

    def flush(self):
        for stream in self.streams:
            stream.flush()


class LogUtil:
    # 日志保存路径
    log_path = ''

    @staticmethod
    def set_log_path(log_path):
        LogUtil.log_path = log_path

    @staticmethod
    def enable():
        if len(LogUtil.log_path) == 0:
            current_file_path = os.path.dirname(os.path.abspath(__file__))
            LogUtil.log_path = os.path.join(current_file_path, 'logs')
            os.makedirs(LogUtil.log_path, exist_ok=True)

        current_date = datetime.datetime.now()
        log_file_name = current_date.strftime("main_%Y%m%d_%H%M.log")
        log_file_path = os.path.join(LogUtil.log_path, log_file_name)
        log_stream = open(log_file_path, 'a')
        sys.stdout = PrintStream([sys.stdout, log_stream])
        sys.stderr = PrintStream([sys.stderr, log_stream])

from lib.cptoml import fetch, put
import os
import gc

class CircuitPythonTOMLConfig:
    def __init__(self):
        self.setter = print
        self.getter = os.getenv

    def set(self, key: str, value):
        self.setter(f'Key: {key}, value: {value}')
    
    def get(self, item):
        return self.getter(item)
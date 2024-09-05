import os


# TODO: Persist settings which are set during runtime
class Settings:
    def __init__(self):
        self.cache = {}

    def set(self, key: str, value):
        self.cache[key] = value
        print(f"Key: {key}, value: {value}")

    def get(self, key: str):
        item = self.cache.get(key)
        if item is None:
            item = os.getenv(key)
            self.cache[key] = item
        return item

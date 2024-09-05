import os


class Settings:
    def set(self, key: str, value):
        print(f"Key: {key}, value: {value}")

    def get(self, item):
        return os.getenv(item)

import os
import adafruit_logging as logging

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)  # type: ignore


# TODO: Persist settings which are set during runtime
class Settings:
    def __init__(self):
        self.cache = {}

    def set(self, key: str, value):
        self.cache[key] = value
        logger.info(f"Setting: {key} = {value}")

    def get(self, key: str):
        item = self.cache.get(key)
        if item is None:
            item = os.getenv(key)
            self.cache[key] = item
        return item

from .device_api import Output
import inspect


def get_caller(outputs, method_name):
    methods = [getattr(output, method_name) for output in outputs]

    def caller(*args):
        for method in methods:
            method(*args)

    return caller


class MultiMethods:
    def __init__(self, outputs: list[Output]):
        method_names = [name for (name, _) in inspect.getmembers(
            Output, predicate=inspect.isfunction)]

        for name in method_names:
            setattr(self, name, get_caller(outputs, name))

class Broadcaster:
    def __init__(self, prototype, children):
        method_names = [
            name for name
            in dir(prototype)
            if not name.startswith("__")]

        for name in method_names:
            setattr(self, name, get_caller(children, name))


def get_caller(children, method_name):
    children_methods = [getattr(child, method_name) for child in children]

    def caller(*args):
        for method in children_methods:
            method(*args)

    return caller

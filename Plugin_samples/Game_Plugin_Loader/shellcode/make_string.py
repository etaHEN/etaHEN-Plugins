import sys

def print_string_def(name: str, data: bytes):
    if len(data) % 8 != 0:
        data = data + b'\x00'*(8 - (len(data) % 8))
    buf = memoryview(data).cast('Q')
    print(f'volatile unsigned long long {name}[{len(buf)}];')
    for i, v in enumerate(buf):
        print(f'{name}[{i}] = 0x{v:016x};')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print(f'usage: {__file__} variable_name string')
        sys.exit(0)
    print_string_def(sys.argv[1], sys.argv[2].encode('latin-1'))

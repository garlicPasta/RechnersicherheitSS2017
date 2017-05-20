import bcrypt
import re
from multiprocessing import Pool

POOL_SIZE = 8


def build_users_from_file():
    users = []
    with open("a4_master.passwd", encoding='UTF-8') as f:
        lines = f.readlines()
        for line in lines:
            users.append(line.split(':'))
    return users


def load_english_dict():
    words = ["correct horse battery staple", "demisemiquaver"]
    return words
    with open("words.txt", encoding='UTF-8', mode='r') as f:
        lines = f.readlines()
        for line in lines:
            words.append(re.sub('(/.*|\n)', '', line))
    return words


def pin_generator():
    for a in range(10):
        for b in range(10):
            for c in range(10):
                for d in range(10):
                    yield str(a) + str(b) + str(c) + str(d)


def calc_hash_for(password, salt):
    return password, bcrypt.hashpw(bytes(password, 'UTF-8'), bytearray(salt, encoding='UTF-8')).decode('latin-1')


pool = Pool(POOL_SIZE)
pin_gen = pin_generator()

found_keys = []
results = []
unknow_users = build_users_from_file()

for user in unknow_users:
    salt = user[1][0:29]
    for pin in pin_generator():
        results.append(pool.apply_async(calc_hash_for, (pin, salt)))

for user in unknow_users:
    salt = user[1][0:29]
    for english_word in load_english_dict():
        results.append(pool.apply_async(calc_hash_for, (english_word, salt)))

for result in results:
    for user in unknow_users:
        password, password_hash = result.get()
        if password_hash == user[1]:
            found_keys.append((user[0], password))
            unknow_users.remove(user)

print(found_keys)

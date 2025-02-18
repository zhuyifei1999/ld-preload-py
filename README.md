Self-contained LD_PRELOAD that runs Python. Contains its own libc (musl), CPython interpreter, and Python stdlib, in just 12 MiB.

```
zhuyifei1999 /tmp $ ls -lh python.so
-rw-r--r-- 1 zhuyifei1999 zhuyifei1999 12M Feb 18  2025 python.so
zhuyifei1999 /tmp $ LD_PRELOAD=./python.so id
Python 3.13.2 (tags/v3.13.2-dirty:4f8bb39, Feb 18 2025, 05:01:52) [GCC 14.2.0] on linux
Type "help", "copyright", "credits" or "license" for more information.
(InteractiveConsole)
>>> import sys
>>> sys.path
['/tmp/python.so', '/usr/lib/python313.zip', '/usr/lib/python3.13', '/usr/lib/python3.13/lib-dynload', '/usr/lib/python3.13/site-packages']
>>> import traceback
>>> traceback.print_stack()
  File "<frozen importlib._bootstrap>", line 1360, in _find_and_load
  File "<frozen importlib._bootstrap>", line 1331, in _find_and_load_unlocked
  File "<frozen importlib._bootstrap>", line 935, in _load_unlocked
  File "<frozen importlib._bootstrap_external>", line 1026, in exec_module
  File "<frozen importlib._bootstrap>", line 488, in _call_with_frames_removed
  File "/tmp/python.so/main.py", line 1, in <module>
    __import__('code').interact()
  File "/tmp/python.so/code.py", line 372, in interact
    console.interact(banner,exitmsg)
  File "/tmp/python.so/code.py", line 272, in interact
    more=self.push(line)
  File "/tmp/python.so/code.py", line 314, in push
    more=self.runsource(source,filename,symbol=_symbol)
  File "/tmp/python.so/code.py", line 76, in runsource
    self.runcode(code)
  File "/tmp/python.so/code.py", line 92, in runcode
    exec(code,self.locals)
  File "<console>", line 1, in <module>
>>>
now exiting InteractiveConsole...
uid=1000(zhuyifei1999) gid=1000(zhuyifei1999) groups=1000(zhuyifei1999)
```

# Logfind

Toy `grep` utility implementation, just for learning C purpose.

## Install

Just `make` and then use binary file.

## Usage

```sh
./logfind INFO FILE    # for searching lines that contains BOTH INFO and FILE
./logfind INFO WARN -o # for searching lines that contains INFO or WARN
```

Reads `.logfind` file which is by default located in `/home/user/.logfind` for searching paths (glob expressions are supported).

.logfind example:

```
~/code/myproject/settings.py
~/code/myproject/**.log
```

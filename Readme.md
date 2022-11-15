如果這個程式對你有幫助，希望你能夠幫我按個star
## How to compile & exec
### compile
```
make
```
### clear executable
```
make clean
```
### exec
>[exe] [port]
```
./np_simple 1234
./np_single_proc 2345
./np_multi_proc 3456
```

## three different socket program
### np_simple (Single user)
- Concurrent connection-oriented
- only allow one client connect
### np_single_proc (Multiple user)
- single-process concurrent chat-like systems
- userPipe with pipe()

### np_multi_proc (Multiple user)
- Concurrent connection-oriented + FIFO + Shared memory
- fork slave server to handle each client
- Shared memory implement clients and broadcast
    - use <mman.h> mmap to handle shared memory
- FIFO implement userPipe
    - use mknod(fifoFileName.c_str(), S_IFIFO | 0777, 0) to create FIFO file


**detail function saw document**

## others may use
### 查找 port 是否有人使用
ex: port:1234
```
$ netstat -napl | grep "1234"
```
### 殺掉只用 port 的 program
ex: port:1234
```
$ sudo kill $(sudo lsof -t -i:1234)
```
### 對某個程式查找為何core dumped (ex: segamentation faults)
```
# 列出core dump的程式
$ coredumpctl

# 查找某個程式的 core dumped 原因 (以 np_multi_proc 這個程式為例)
$ coredumpctl debug np_multi_proc
```

### 查找 shared memory
```
$ ipcs -m

# 清理
$ ipcrm -m [shmid]
```

### clear .nfs cache檔
```
# 假設.nfs檔名= .nfs001
$ lsof .nfs001 | awk '{print $2}' | xargs -rn1 kill
```
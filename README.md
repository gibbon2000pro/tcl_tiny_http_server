# tcl_tiny_http_server
一个简单的tcl的http server，按照自己的使用习惯进行设计

### 创建此项目的目的有三条：

1. 能在简历上写自己有独立开源工程的经历

2. 方便投递个人简历。（只需要发送链接 http://47.108.224.68:8080/gibbon 即可）

3. 解密挑战，“Rita同学的属相是什么？”


```bash
LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libasan.so.6 ./demo_resume_server.tcl >record &2>1
```

```bash
fallocate -l 8G ./swapfile
chmod 0600 ./swapfile
mkswap ./swapfile
swapon ./swapfile
```
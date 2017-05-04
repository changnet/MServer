#!/bin/bash

# https://docs.docker.com/engine/installation/linux/ubuntu/

$ sudo apt-get install \
    apt-transport-https \
    ca-certificates \
    curl \
    software-properties-common

curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo apt-key add -

sudo apt-key fingerprint 0EBFCD88

sudo add-apt-repository \
   "deb [arch=amd64] https://download.docker.com/linux/ubuntu \
   $(lsb_release -cs) \
   stable"

sudo apt-get update

# Unless you have a strong reason not to, install the linux-image-extra-* 
# packages, which allow Docker to use the aufs storage drivers.
sudo apt-get install \
    linux-image-extra-$(uname -r) \
    linux-image-extra-virtual


sudo apt-get install docker-ce

# test
sudo docker run hello-world

# pull a image
# sudo docker pull debian:7

# http://opslinux.com/2016/09/28/%E4%BD%BF%E7%94%A8docker%E6%90%AD%E5%BB%BA%E5%BC%80%E5%8F%91%E7%8E%AF%E5%A2%83/

# docker run [OPTIONS] IMAGE [COMMAND] [ARG...]
# docker run -ti -h dev --net=host -v ~/workspace/sohu:/root/workspace -w /root develop:base /bin/bash
# -i --interactive,Keep STDIN open even if not attached
# -t Allocate a pseudo-TTY
# --hostname, -h Container host name
# --net Connect a container to a 
# --volume, -v	 	Bind mount a volume 自动挂载目录
# --workdir, -w	 	Working directory inside the container 设置程序运行根目录
# develop:base IMAGE，容器名,develop是REPOSITORY名，base是TAG名，用docker images会显示出来

# docker commit [OPTIONS] CONTAINER [REPOSITORY[:TAG]]  Create a new image from a container’s changes

# -it 以交互模式运行
# -h dev 把当前运行的container命名为dev
# -v ~/github:/root/workspace 把主机目录~/github挂载到container中的/root/workspace
# debian:7  运行这个image,如果Image不存在，则会docker hub下载
# /bin/bash 在container中运行的主程序是bash这个shell

sudo docker run -it -h dev --net=host -v ~/github:/root/workspace debian:7 /bin/bash

# xzc@xzc-VirtualBox:~/github/MServer$ sudo docker run -it -h dev --net=host -v ~/github:/root/workspace debian:7 /bin/bash
# [sudo] password for xzc: 
# root@dev:/# pwd
# /
# root@dev:/# cd root/workspace/
# root@dev:~/workspace# ls
# MServer  lua_flatbuffers
# root@dev:~/workspace# 

# 挂载的目录权限是继承主系统的，ubuntu默认是不能运行sh文件的。debian则可以
root@dev:~/workspace/MServer/shell# ./build_env.sh
bash: ./build_env.sh: Permission denied
root@dev:~/workspace/MServer/shell# chmod +x *

# 发现找不到基础的软件包
root@dev:/# apt-get install gcc
Reading package lists... Done
Building dependency tree       
Reading state information... Done
E: Unable to locate package gcc

# 查看下软件源
root@dev:/# cat /etc/apt/sources.list
deb http://deb.debian.org/debian wheezy main
deb http://deb.debian.org/debian wheezy-updates main
deb http://security.debian.org wheezy/updates main

# 修改软件源，发现基础的编辑器一个不带
root@dev:/# vi /etc/apt/sources.list
bash: vi: command not found
root@dev:/# nano /etc/apt/sources.list
bash: nano: command not found
root@dev:/# 

# 先更新下源，安装编辑器
root@dev:/# apt-get update

# 安装vim并修改源
root@dev:/# apt-get install vim
vi /etc/apt/sources.list

# deb http://mirrors.163.com/debian/ wheezy main non-free contrib
# deb http://mirrors.163.com/debian/ wheezy-updates main non-free contrib
# deb http://mirrors.163.com/debian/ wheezy-backports main non-free contrib
# deb-src http://mirrors.163.com/debian/ wheezy main non-free contrib
# deb-src http://mirrors.163.com/debian/ wheezy-updates main non-free contrib
# deb-src http://mirrors.163.com/debian/ wheezy-backports main non-free contrib
# deb http://mirrors.163.com/debian-security/ wheezy/updates main non-free contrib
# deb-src http://mirrors.163.com/debian-security/ wheezy/updates main non-free contrib

# 重新更新源
root@dev:/# apt-get update

# 删除不必要的软件包缓存
root@dev:~/workspace/MServer/shell# apt-get clean
root@dev:~/workspace/MServer/shell# apt-get clean all
root@dev:~/workspace/MServer/shell# apt-get autoremove
Reading package lists... Done
Building dependency tree       
Reading state information... Done
0 upgraded, 0 newly installed, 0 to remove and 0 not upgrade

# 退出docker
root@dev:~/workspace/MServer/shell# exit
exit

# 查看刚刚生成的docker，0f6f6b145007就是
xzc@xzc-VirtualBox:~/github/MServer$ sudo docker ps -a
[sudo] password for xzc: 
CONTAINER ID        IMAGE               COMMAND             CREATED             STATUS                      PORTS               NAMES
0f6f6b145007        debian:7            "/bin/bash"         About an hour ago   Exited (0) 42 seconds ago                       frosty_aryabhata
797d2f0929b4        hello-world         "/hello"            3 hours ago         Exited (0) 3 hours ago                          zen_brown
a9550ead32dc        hello-world         "/hello"            25 hours ago        Exited (0) 25 hours ago                         musing_dijkstra
xzc@xzc-VirtualBox:~/github/MServer$

# 保存docker，这个时间会长一点，取决于你在docker中放了多少内容
xzc@xzc-VirtualBox:~/github/MServer$ sudo docker commit 0f6f6b145007 dev
sha256:5379829de3d5f679242f1ee1c52bc1fca2f84298ad3515ad55688713fc24c996

# 查看刚刚保存的image
xzc@xzc-VirtualBox:~/github/MServer$ sudo docker images
REPOSITORY          TAG                 IMAGE ID            CREATED             SIZE
dev                 latest              5379829de3d5        14 seconds ago      337 MB
debian              7                   e47e01d8724c        9 days ago          85.3 MB
hello-world         latest              48b5124b2768        3 months ago        1.84 kB

# 如果你需要在不同地方使用，或者要给同事使用，打包带走
sudo docker save -o ~/MServer.tar dev

# 加载
docker load -i ~/MServer.tar


# 在部署过程中可能会出错，这时可以直接退出docker，找到不用的，直接删掉重来
xzc@xzc-VirtualBox:~/github/MServer$ sudo docker ps -a
CONTAINER ID        IMAGE               COMMAND             CREATED             STATUS                         PORTS               NAMES
8c6e997dc885        debian:7            "/bin/bash"         8 minutes ago       Exited (127) 3 minutes ago                         zealous_roentgen
797d2f0929b4        hello-world         "/hello"            About an hour ago   Exited (0) About an hour ago                       zen_brown
a9550ead32dc        hello-world         "/hello"            23 hours ago        Exited (0) 23 hours ago                            musing_dijkstra
xzc@xzc-VirtualBox:~/github/MServer$ sudo docker rm 8c6e997dc885

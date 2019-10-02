#!/bin/bash

set -e
set -o pipefail

OS_IMAGE=debian:9
DOCKER_NAME=mserver
DOCKER_DIR=-v /home/xzc/Documents/code/MServer:/root/MServer

# https://docs.docker.com/engine/installation/linux/ubuntu/

# 更换国内镜像源：http://mirrors.ustc.edu.cn/help/dockerhub.html?highlight=docker
# 对于使用 upstart 的系统（Ubuntu 14.04、Debian 7 Wheezy），在配置文件 /etc/default/docker 中的 DOCKER_OPTS 中配置Hub地址：

# DOCKER_OPTS="--registry-mirror=https://docker.mirrors.ustc.edu.cn/"
# 重新启动服务:

# sudo service docker restart
# 对于使用 systemd 的系统（Ubuntu 16.04+、Debian 8+、CentOS 7）， 在配置文件 /etc/docker/daemon.json 中加入：

# {
#   "registry-mirrors": ["https://docker.mirrors.ustc.edu.cn/"]
# }
# 重新启动 dockerd：

# sudo systemctl restart docker

# 检查 Docker Hub 是否生效
# 在命令行执行 docker info ，如果从结果中看到了如下内容，说明配置成功。

# Registry Mirrors:
#     https://docker.mirrors.ustc.edu.cn/


## 直接从镜像pull，不用设置源(Azure中国镜像:dockerhub.azk8s.cn)
# sudo docker pull dockerhub.azk8s.cn/library/mariadb
# sudo docker pull dockerhub.azk8s.cn/library/mongo

# ustc的源里没有mysql，sudo docker pull docker.mirrors.ustc.edu.cn/library/mariadb
# 这样是不行的

# 安装docker环境
function build_docker_env()
{
    sudo apt-get install \
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
}

# 安装一个完整开发环境的docker，包含mongodb等。
# 最好不要这样用，mongodb和mariadb是分开不同的docker了
function build_docker()
{
    # pull a image
    sudo docker pull $OS_IMAGE

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

    sudo docker run -it -h dev --net=host -v $DOCKER_DIR $OS_IMAGE /bin/bash

    # 清空软件源
    > /etc/apt/sources.list
    # 修改软件源
    echo deb http://mirrors.163.com/debian/ stretch main non-free contrib > /etc/apt/sources.list
    echo deb http://mirrors.163.com/debian/ stretch-updates main non-free contrib >> /etc/apt/sources.list
    echo deb http://mirrors.163.com/debian/ stretch-backports main non-free contrib >> /etc/apt/sources.list
    echo deb-src http://mirrors.163.com/debian/ stretch main non-free contrib >> /etc/apt/sources.list
    echo deb-src http://mirrors.163.com/debian/ stretch-updates main non-free contrib >> /etc/apt/sources.list
    echo deb-src http://mirrors.163.com/debian/ stretch-backports main non-free contrib >> /etc/apt/sources.list
    echo deb http://mirrors.163.com/debian-security/ stretch/updates main non-free contrib >> /etc/apt/sources.list
    echo deb-src http://mirrors.163.com/debian-security/ stretch/updates main non-free contrib >> /etc/apt/sources.list

    # 重新更新源
    sudo apt-get update

    # 默认是没有运行权限的，需要加上
    cd root/workspace/MServer/shell
    chmod +x *
    # 安装开发环境
    ./build_env.sh

    # 删除不必要的软件包缓存
    apt-get clean
    apt-get clean all
    apt-get autoremove

    # 退出docker
    exit
}

# 打包一个docker
function build_docker_image()
{
    # sudo docker ps -a查看刚刚生成的docker，记住CONTAINER ID，如0f6f6b145007
    # CONTAINER ID        IMAGE               COMMAND             CREATED             STATUS                      PORTS               NAMES
    # 0f6f6b145007        debian:7            "/bin/bash"         About an hour ago   Exited (0) 42 seconds ago                       frosty_aryabhata
    # 797d2f0929b4        hello-world         "/hello"            3 hours ago         Exited (0) 3 hours ago                          zen_brown
    # a9550ead32dc        hello-world         "/hello"            25 hours ago        Exited (0) 25 hours ago                         musing_dijkstra

    # 保存docker，这个时间会长一点，取决于你在docker中放了多少内容
    sudo docker commit $1 dev


    # sudo docker images查看刚刚保存的image
    # xzc@xzc-VirtualBox:~/github/MServer$ sudo docker images
    # REPOSITORY          TAG                 IMAGE ID            CREATED             SIZE
    # dev                 latest              5379829de3d5        14 seconds ago      337 MB
    # debian              7                   e47e01d8724c        9 days ago          85.3 MB
    # hello-world         latest              48b5124b2768        3 months ago        1.84 kB

    # 如果你需要在不同地方使用，或者要给同事使用，打包带走
    # $2为保存的路径，如sudo docker save -o ~/MServer_dev_docker.tar dev
    sudo docker save -o $2 dev
}

# 加载上面打包的docker
function load_docker()
{
    # 加载
    # load_docker ~/MServer_dev_docker.tar
    sudo docker load -i $1
}

# 初始化docker部署环境，注意不是开发环境
function init_docker_deploy()
{
    # --net=host 使用本机网络(默认docker会创建一个桥接网络，程序是运行在里面的)

    # 对于从mirror拉取的docker，格式是：madockerhub.azk8s./library/mariadb
    # 用官方的方式启动不行，提示：Unable to find image 'madockerhub.azk8s.cn/library/mariadb:latest' locally
    # 换成 IMAGE ID 才可以

    # mariadb的docker参数在这里:https://hub.docker.com/_/mariadb/
    # https://mariadb.com/kb/en/library/documentation/mariadb-administration/getting-installing-and-upgrading-mariadb/binary-packages/installing-and-using-mariadb-via-docker/
    # MYSQL_ROOT_PASSWORD:root用户密码，必须有
    # 注意mariadb在debian下root用户是写死了没密码的，你设置啥都没用。详见mysql.sh
    sudo docker run --net=host -v $DOCKER_DIR --name $DOCKER_NAME"_mariadb" -e MYSQL_ROOT_PASSWORD=1 -d mariadb/server:latest

    # https://hub.docker.com/_/mongo
    # mongod默认是不读配置的，都使用参数，我们要么挂载一个配置文件上去，要么使用样本
    # docker的日志：sudo docker logs mserver_mongodb
    # mongodb没法设置port，因此使用桥接网络，映射到主机。用交互模式时，注意要用原端口27017
    sudo docker run -p 27013:27017 -v $DOCKER_DIR --name $DOCKER_NAME"_mongodb" -d mongo:latest

    sudo docker run -it --net=host --name $DOCKER_NAME"_debian" -v $DOCKER_DIR $OS_IMAGE /bin/bash
}

# 启动部署好的docker
function start()
{
    sudo docker start $DOCKER_NAME"_mariadb"
    sudo docker start $DOCKER_NAME"_mongodb"
    sudo docker start $DOCKER_NAME"_debian"

    # 启动后，直接用名字交互就可以了 sudo docker exec -it mserver_mariadb /bin/bash
}

# 关闭运行中的docker
function stop()
{
    sudo docker stop $DOCKER_NAME"_debian"
    sudo docker start $DOCKER_NAME"_mariadb"
    sudo docker start $DOCKER_NAME"_mongodb"
}


# 在部署过程中可能会出错，这时可以直接退出docker，找到不用的，直接删掉重来
# xzc@xzc-VirtualBox:~/github/MServer$ sudo docker ps -a
# CONTAINER ID        IMAGE               COMMAND             CREATED             STATUS                         PORTS               NAMES
# 8c6e997dc885        debian:7            "/bin/bash"         8 minutes ago       Exited (127) 3 minutes ago                         zealous_roentgen
# 797d2f0929b4        hello-world         "/hello"            About an hour ago   Exited (0) About an hour ago                       zen_brown
# a9550ead32dc        hello-world         "/hello"            23 hours ago        Exited (0) 23 hours ago                            musing_dijkstra
# xzc@xzc-VirtualBox:~/github/MServer$ sudo docker rm 8c6e997dc885
# 
# 如果要删掉image，用的是rmi，如:sudo docker rmi 8c6e997dc885
# 
# 进入已经运行的docker（shell多开）
# xzc@xzc-HP-ProBook-4446s:~/Documents/code/MServer$ sudo docker ps -a
# CONTAINER ID        IMAGE               COMMAND             CREATED             STATUS                    PORTS               NAMES
# 0b1e2491aa41        dev                 "/bin/bash"         2 hours ago         Up 2 hours                                    nostalgic_hoover
# 1681557aceae        hello-world         "/hello"            3 months ago        Exited (0) 3 months ago                       youthful_almeida
# xzc@xzc-HP-ProBook-4446s:~/Documents/code/MServer$ sudo docker exec -it 0b1e2491aa41 /bin/bash
# root@dev:/#

#!/bin/bash

set -e
set -o pipefail

# https://stackoverflow.com/questions/2853803/in-a-shell-script-echo-shell-commands-as-they-are-executed
set -x

# openssl generate CA certificate and private key and client certificate

# from:https://yq.aliyun.com/articles/40398
# maybe:https://my.oschina.net/itblog/blog/651434?spm=5176.100239.blogcont40398.25.mdIPpV

# 一些概念的解释：https://www.cnblogs.com/guogangj/p/4118605.html
# X.509 - 这是一种证书标准,主要定义了证书中应该包含哪些内容.其详情可以参考RFC5280,SSL使用的就是这种证书标准
# PEM - Privacy Enhanced Mail,打开看文本格式,以"-----BEGIN..."开头, "-----END..."结尾,内容是BASE64编码.Apache和*NIX服务器偏向于使用这种编码格式.
# DER - Distinguished Encoding Rules,打开看是二进制格式,不可读.
# CRT - CRT应该是certificate的三个字母,其实还是证书的意思,常见于*NIX系统,有可能是PEM编码,也有可能是DER编码,大多数应该是PEM编码,相信你已经知道怎么辨别.
# CER - 还是certificate,还是证书,常见于Windows系统,同样的,可能是PEM编码,也可能是DER编码,大多数应该是DER编码.
# KEY - 通常用来存放一个公钥或者私钥,并非X.509证书,编码同样的,可能是PEM,也可能是DER.
# CSR - Certificate Signing Request,即证书签名请求,这个并不是证书,而是向权威证书颁发机构获得签名证书的申请,其核心内容是一个公钥(当然还附带了一些别的信息),在生成这个申请的时候,同时也会生成一个私钥,私钥要自己保管好

# 用于测试的密码
OPENSSLPASS=mini_distributed_game_server

# 用：locate openssl.cnf来确定openssl的配置文件，
# 创建各个工作目录，如：dir		= ./demoCA		# TSA root directory
# newcerts——存放CA指令生成的新证书
# private——存放私钥
mkdir -p ./demoCA/{private,public}
cd ./demoCA

# index.txt——OpenSSL定义的已签发证书的文本数据库文件，这个文件通常在初始化的时候是空的
# serial——证书签发时使用的序列号参考文件，该文件的序列号是以16进制格式进行存放的，该文
# 件必须提供并且包含一个有效的序列号
touch index.txt
echo 01 > serial

# 生成证书之前，需要先生成一个随机数
# rand——生成随机数
# -out——指定输出文件
# 1000——指定随机数长度
openssl rand -out private/.rand 1000

############ 根证书 ##########################
# 普通的证书是证书颁发机构（CA）用他们的根证书签发的。现在我们不需要认证，因此需要自己
# 先产生一个根证书，给自己签发。
# https://zh.wikipedia.org/wiki/%E6%A0%B9%E8%AF%81%E4%B9%A6

# 生成根证书私钥pem文件(Privacy Enbanced Mail)
# genrsa——使用RSA算法产生私钥
# -aes256——使用256位密钥的AES算法对私钥进行加密
# -out——输出文件的路径
# 1024——指定私钥长度
# -passout－密码输入方式，pass表示为明文指定密码
# !!! 选择了加密方式，则必须要输入密码，不选择则可以无密码
openssl genrsa -aes256 -out private/ca_key.pem -passout pass:$OPENSSLPASS 1024

# 使用私钥生成根证书签发申请文件(csr文件)
# 证书要向证书颁发机构申请的，要向他们提交申请文件。这个文件包含申请组织的信息
# req——执行证书签发命令
# -new——新证书签发请求
# -key——指定私钥路径
# -out——输出的csr文件的路径
# -subj——证书相关的用户信息(subject的缩写)，如果没有这个字段，在生成过程中会要求填写
#        C  => Country
#        ST => State
#        L  => City
#        O  => Organization
#        OU => Organization Unit
#        CN => Common Name (证书所请求的域名,访问时，这个域名要匹配才能认证)
openssl req -new -key private/ca_key.pem -passin pass:$OPENSSLPASS -out private/ca.csr -subj \
"/C=CN/ST=Guangzhou/L=Guangzhou/O=MiniDistributedGameServer/OU=changnet/CN=github.com#changnet"

# 上一步自己向自己提交了证书申请，现在自签
# x509——生成x509格式证书
# req——输入csr文件
# days——证书的有效期（天）
# sha1——证书摘要采用sha1算法
# extensions——按照openssl.cnf文件中配置的v3_ca项添加扩展
# signkey——签发证书的私钥
# in——要输入的csr文件
# out——输出的cer证书文件
openssl x509 -req -days 3650 -sha1 -extensions v3_ca -signkey \
private/ca_key.pem -passin pass:$OPENSSLPASS -in private/ca.csr -out public/ca.cer

##################### 根证书完成 #################################
# 现在我们拥有一张经过签发的合法根证书了，可以给别人签发证书了



################### 服务端证书 ###############################
# 生成服务端私钥
openssl genrsa -aes256 -out private/srv_key.pem -passout pass:$OPENSSLPASS 1024
# 申请
openssl req -new -key private/srv_key.pem -passin pass:$OPENSSLPASS -out private/server.csr -subj \
"/C=CN/ST=Guangzhou/L=Guangzhou/O=MiniDistributedGameServer/OU=server/CN=minidistributedgameserver.com"
# 签发
# CA——指定CA证书的路径
# CAkey——指定CA证书的私钥路径
# CAserial——指定证书序列号文件的路径
# CAcreateserial——表示创建证书序列号文件(即上方提到的serial文件)，创建的序列号文件默认名称为-CA，指定的证书名称后加上.srl后缀
# 注意：这里指定的-extensions的值为v3_req，在OpenSSL的配置中，v3_req配置的basicConstraints的值为CA:FALSE
# 而前面生成根证书时，使用的-extensions值为v3_ca，v3_ca中指定的basicConstraints的值为CA:TRUE，表示该证书是颁发给CA机构的证书
openssl x509 -req -days 3650 -sha1 -extensions v3_req -CA public/ca.cer -passin pass:$OPENSSLPASS -CAkey private/ca_key.pem \
-CAserial ca.srl -CAcreateserial -in private/server.csr -out public/server.cer


################################## 客户端证书 ###############################
# 通常情况下，我们不需要客户端证书。比如https网站，用户只需要服务端证书来验证服务器是
# 合法的即可，但服务器并不在乎谁来访问。
# 但是在一些安全性要求较的系统。比如公司内部OA系统，要求验证客户端的合法性。比如,一个
# 员工需要在家里登录OA系统，那服务器必须验证这个用户为公司自己的员工。

# 生成客户端私钥
openssl genrsa -aes256 -out private/clt_key.pem -passout pass:$OPENSSLPASS 1024
# 申请
openssl req -new -key private/clt_key.pem -passin pass:$OPENSSLPASS -out private/client.csr -subj \
"/C=CN/ST=Guangzhou/L=Guangzhou/O=MiniDistributedGameServer/OU=client/CN=minidistributedgameserver.com"
# 签发
# 注意上方签发服务端证书时已经使用-CAcreateserial生成过ca.srl文件，因此这里不需要带上这个参数了
openssl x509 -req -days 3650 -sha1 -extensions v3_req -CA public/ca.cer -passin pass:$OPENSSLPASS -CAkey private/ca_key.pem \
-CAserial ca.srl -in private/client.csr -out public/client.cer


#可以用下面的方法测试生成的key和证书
# openssl rsa -text -in private/srv_key.pem -passin pass:$OPENSSLPASS
# openssl x509 -in public/server.cer -text

# 可用以下方法去掉密码
openssl rsa -in private/srv_key.pem -passin pass:$OPENSSLPASS -out private/srv_key_no_passwd.pem

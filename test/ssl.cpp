//https://www.openssl.org/docs/manmaster/
//http://www.ibm.com/developerworks/library/l-openssl/
//https://www.openssl.org/docs/manmaster/ssl/
//http://www.2cto.com/kf/201503/385900.html
//http://stackoverflow.com/questions/11705815/client-and-server-communication-using-ssl-c-c-ssl-protocol-dont-works

HttpsClient::HttpsClient(void):
             wsaData(NULL),
             socketAddrClient(NULL),
             ssl(NULL),
             sslCtx(NULL),
             sslMethod(NULL),
             serverCertification(NULL)
{
    SSL_load_error_strings();
    SSLeay_add_ssl_algorithms();
}

HttpsClient::~HttpsClient(void)
{
    //!清理打开的句柄
    if (NULL != ssl)
    {
        SSL_shutdown(ssl);
        closesocket(socketClient);
        SSL_free(ssl);
        ssl = NULL;
    }

    if (NULL != sslCtx)
    {
        SSL_CTX_free(sslCtx);
    }

    WSACleanup();
}

BOOL HttpsClient::ConnectToServer(const CString strServerUrl, const int nPort)
{
    cstrServerUrl = strServerUrl;
    nServerPort = nPort;
    BOOL bRet = FALSE;

    do
    {
        if (!InitializeSocketContext())
        {
            break;
        }

        if (!SocketConnect())
        {
            break;
        }

        if (!InitializeSslContext())
        {
            break;
        }

        if (!SslConnect())
        {
            break;
        }

        bRet = TRUE;
    } while (FALSE);
    return bRet;
}

BOOL HttpsClient::LoginToServer(const CString strUsername, const CString strPasswd)
{
    cstrUserName = strUsername;
    cstrPassWord = strPasswd;
    BOOL bRet = FALSE;

    do
    {
        if (!SendLoginPostData())
        {
            break;
        }

        CString cstrRecvData;
        RecvLoginPostData(cstrRecvData);
        if (cstrRecvData.GetLength() == 0)
        {
            break;
        }

        ParseCookieFromRecvData(cstrRecvData);

        if (cstrCookieUid.IsEmpty() || cstrCookieUid.Compare(EXPIRED) == 0)
        {
            break;
        }

        bRet = TRUE;
    } while (FALSE);
    return bRet;
}

BOOL HttpsClient::LogoutOfServer()
{
    return FALSE;
}

BOOL HttpsClient::InitializeSocketContext()
{
    //!初始化winSocket环境
    BOOL bRet = FALSE;
    wsaData = new WSADATA;
    WORD wVersion = MAKEWORD(2, 2);

    do
    {
        if(0 != WSAStartup(wVersion, wsaData))
        {
            break;
        }

        if(LOBYTE( wsaData->wVersion ) != 2 || HIBYTE( wsaData->wVersion ) != 2 )
        {
            WSACleanup();
            break;
        }

        LPHOSTENT lpHostTent;
        lpHostTent = gethostbyname(cstrServerUrl);
        if (NULL == lpHostTent)
        {
            break;
        }

        socketClient = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socketClient == INVALID_SOCKET)
        {
            WSACleanup();
            break;
        }

        socketAddrClient = new SOCKADDR_IN;
        socketAddrClient->sin_family = AF_INET;
        socketAddrClient->sin_port = htons(nServerPort);
        socketAddrClient->sin_addr = *((LPIN_ADDR)*lpHostTent->h_addr_list);
        memset(socketAddrClient->sin_zero, 0, sizeof(socketAddrClient->sin_zero));

        bRet = TRUE;
    } while (FALSE);

    return bRet;
}

BOOL HttpsClient::SocketConnect()
{
    //!原生socket连接
    BOOL bRet = FALSE;

    do
    {
        if (SOCKET_ERROR == connect(socketClient, (LPSOCKADDR)socketAddrClient, sizeof(SOCKADDR_IN)))
        {
            int nErrorCode = WSAGetLastError();
            closesocket(socketClient);
            break;
        }

        bRet = TRUE;
    } while (FALSE);

    return bRet;
}

BOOL HttpsClient::InitializeSslContext()
{
    //!SSL通信初始化
    BOOL bRet = FALSE;

    do
    {
        sslMethod = SSLv23_client_method();
        if(NULL == sslMethod)
        {
            break;
        }

        sslCtx = SSL_CTX_new(sslMethod);
        if (NULL == sslCtx)
        {
            break;
        }

        ssl = SSL_new(sslCtx);
        if (NULL == ssl)
        {
            break;
        }

        bRet = TRUE;
    } while (FALSE);

    return bRet;
}

BOOL HttpsClient::SslConnect()
{
    //!SSL绑定原生socket,并连接服务器
    BOOL bRet = FALSE;

    do
    {
        SSL_set_fd(ssl, socketClient);

        int nRet = SSL_connect(ssl);
        if (-1 == nRet)
        {
            break;
        }

        bRet = TRUE;
    } while (FALSE);

    return bRet;
}

BOOL HttpsClient::SslGetCipherAndCertification()
{
    BOOL bRet = FALSE;

    do
    {
        cstrSslCipher = SSL_get_cipher(ssl);
        serverCertification = SSL_get_certificate(ssl);

        if (NULL == serverCertification)
        {
            break;
        }

        cstrSslSubject = X509_NAME_oneline(X509_get_subject_name(serverCertification), 0, 0);
        cstrSslIssuer = X509_NAME_oneline(X509_get_issuer_name(serverCertification), 0, 0);

        X509_free(serverCertification);

        bRet = TRUE;
    } while (FALSE);

    return bRet;
}

BOOL HttpsClient::SendLoginPostData()
{
    CString cstrSendData;
    //CString cstrSendParam = redirect=&username=+cstrUserName+&password=+cstrPassWord+&auto_login=checked&submit=%E7%99%BB%E5%BD%95;
    CString cstrSendParam = user=+cstrUserName+&_json=true&pwd=+cstrPassWord+&callback=http%3A%2F%2Forder.mi.com%2Flogin%2Fcallback%3Ffollowup%3Dhttp%253A%252F%252Fwww.mi.com%252F%26sign%3DNWU4MzRmNjBhZmU4MDRmNmZkYzVjMTZhMGVlMGFmMTllMGY0ZTNhZQ%2C%2C&sid=mi_eshop&qs=%253Fcallback%253Dhttp%25253A%25252F%25252Forder.mi.com%25252Flogin%25252Fcallback%25253Ffollowup%25253Dhttp%2525253A%2525252F%2525252Fwww.mi.com%2525252F%252526sign%25253DNWU4MzRmNjBhZmU4MDRmNmZkYzVjMTZhMGVlMGFmMTllMGY0ZTNhZQ%25252C%25252C%2526sid%253Dmi_eshop&hidden=&_sign=%2Bw73Dr7cAfRlMfOR6fW%2BF0QG4jE%3D&serviceParam=%7B%22checkSafePhone%22%3Afalse%7D&captCode=;
    BOOL bRet = FALSE;

    CString cstrSendParamLen;
    cstrSendParamLen.Format(%d, cstrSendParam.GetLength());

    cstrSendData = POST https://account.xiaomi.com/pass/serviceLoginAuth2 HTTP/1.1
;
    cstrSendData += Host: account.xiaomi.com
;
    cstrSendData += User-Agent: Mozilla/5.0 (Windows NT 6.3; WOW64; rv:35.0) Gecko/20100101 Firefox/35.0
;
    cstrSendData += Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8
;
    cstrSendData += Accept-Language: zh-cn,zh;q=0.8,en-us;q=0.5,en;q=0.3
;
    cstrSendData += Accept-Encoding: gzip, deflate
;
    cstrSendData += DNT: 1
;
    cstrSendData += Content-Type: application/x-www-form-urlencoded; charset=UTF-8
;
    cstrSendData += Referer: https://account.xiaomi.com/pass/serviceLogin?callback=http%3A%2F%2Forder.mi.com%2Flogin%2Fcallback%3Ffollowup%3Dhttp%253A%252F%252Fwww.mi.com%252Findex.html%26sign%3DNDRhYjQwYmNlZTg2ZGJhZjI0MTJjY2ZiMTNiZWExODMwYjkwNzg2ZQ%2C%2C&sid=mi_eshop
;
    cstrSendData += Content-Length:  + cstrSendParamLen +
;
    cstrSendData += Connection: keep-alive
;
    cstrSendData +=
;
    cstrSendData += cstrSendParam;

    CString cstrRecvData;
    do
    {
        int nRet = SSL_write(ssl, cstrSendData, cstrSendData.GetLength());

        if(-1 == nRet)
        {
            break;
        }

        bRet = TRUE;
    } while (FALSE);

    return bRet;
}

void HttpsClient::RecvLoginPostData(CString &cstrRecvData)
{
    BOOL bRet = FALSE;

    do
    {
        TIMEVAL tval;
        tval.tv_sec = 20;
        tval.tv_usec = 0;

        while(TRUE)
        {
            FD_SET fds;
            FD_ZERO(&fds);
            FD_SET(socketClient, &fds);

            char recvData[1000] = {0};
            int nRecvLen;

            //int nSelect = select(FD_SETSIZE, &fds, NULL, NULL, &tval);
            //if (1 != nSelect)
            //{
            //  break;
            //}

            int nErr = SSL_read(ssl, recvData, sizeof(recvData));
            if (nErr <= 0)
            {
                break;
            }

            cstrRecvData += recvData;
        }

        if (cstrRecvData.GetLength() == 0)
        {
            break;
        }

        bRet = TRUE;
    } while (FALSE);
}

void HttpsClient::ParseCookieFromRecvData(const CString cstrRecvData)
{
    list<cstring> lstCookiesLine;        //!存放Set-Cookie的一行，例：Set-Cookie: vso_uname=houqd_1111;
    CString cstrFind = Set-Cookie:;    //!查找标记
    CString cstrSeperator =
;      //!以
分割号来分割字符串

    int nPos = 0;
    int nStart = cstrRecvData.Find(cstrSeperator);

    while(nStart != -1)
    {
        CString cstrSessionLine = cstrRecvData.Mid(nPos, nStart - nPos + 1);

        if (cstrSessionLine.Find(cstrFind) != -1)
        {
            CString cstrRealRecord = cstrSessionLine.Right(cstrSessionLine.GetLength() - cstrFind.GetLength() - 3);
            list<cstring>::iterator it = find(lstCookiesLine.begin(), lstCookiesLine.end(), cstrRealRecord);
            if (it == lstCookiesLine.end())
            {
                lstCookiesLine.push_back(cstrRealRecord);
            }
        }

        nPos = nStart;
        nStart = cstrRecvData.Find(cstrSeperator, nPos + 2);
    }

    //!根据每行获取的cookie值，解析为key-value的形式
    vector<cstring> vecCookieSet;
    for (list<cstring>::iterator it = lstCookiesLine.begin(); it != lstCookiesLine.end(); it++)
    {
        CString cstrCookies = *it;
        CString cstrSeperator = ;;
        StaticUtility::StringSplit(cstrCookies, cstrSeperator, vecCookieSet);
    }

    vector<cstring> vecTemp;
    for (vector<cstring>::iterator it = vecCookieSet.begin(); it != vecCookieSet.end(); it++)
    {
        vecTemp.clear();
        CString cstrOneCookies = *it;
        CString cstrSeperator = =;

        StaticUtility::StringSplit(cstrOneCookies, cstrSeperator, vecTemp);
        CString cstrKey;
        CString cstrVal;

        if (vecTemp.size() == 2)
        {
            cstrKey = vecTemp[0];
            cstrVal = vecTemp[1];
        }

        if(cstrKey.Compare(userId) == 0)
        {
            cstrCookieUid = cstrVal;
            break;
        }
    }
}

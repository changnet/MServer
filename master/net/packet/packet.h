#ifndef __PACKET_H__
#define __PACKET_H__

/* socket packet parser and deparser */
class packet
{
public:
    packet();
    virtual ~packet();

    virtual int32 parser();
    virtual int32 deparser();
};

#endif /* __PACKET_H__ */

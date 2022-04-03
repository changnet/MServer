const char *__BACKEND__ = "IOCP";

/// backend using iocp implement
class FinalBackend final : public EVBackend
{
public:
    FinalBackend();
    ~FinalBackend();

    bool stop();
    bool start(class EV *ev);
    void wake();
    void backend();
    void modify(int32_t fd, EVIO *w);
};

 FinalBackend::FinalBackend()
{
}

FinalBackend::~FinalBackend()
{
}

bool FinalBackend::stop()
{
    return true;
}

bool FinalBackend::start(class EV *ev)
{
    assert(false);
    return true;
}

void FinalBackend::wake()
{
}
void FinalBackend::backend()
{
}

void FinalBackend::modify(int32_t fd, EVIO *w)
{
}
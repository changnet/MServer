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
    void modify(int32_t fd, int32_t old_ev, int32_t new_ev);
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

void FinalBackend::modify(int32_t fd, int32_t old_ev, int32_t new_ev)
{
}
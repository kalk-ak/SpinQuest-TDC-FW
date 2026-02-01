#ifndef DEBUG

#include "UniqueFD.h"

#endif // !DEBUG

UniqueFD::UniqueFD(int f) : fd(f)
{
}

UniqueFD::UniqueFD(UniqueFD &&other) noexcept : fd(other.release())
{
}

UniqueFD &UniqueFD::operator=(UniqueFD &&other) noexcept
{
    reset(other.release());
    return *this;
}

UniqueFD::~UniqueFD()
{
    reset();
}

void UniqueFD::reset(int new_fd)
{
    if (fd != -1)
        close(fd);
    fd = new_fd;
}

int UniqueFD::get() const
{
    return fd;
}

bool UniqueFD::is_valid() const
{
    return fd != -1;
}

int UniqueFD::release()
{
    int temp = fd;
    fd = -1;
    return temp;
}

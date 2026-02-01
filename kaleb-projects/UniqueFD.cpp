#include <unistd.h>

// NOTE: The reason why UniqueFD is needed is
// 1. To ensure that files are properly closed when they go out of scope (RAII philosophy)
// 2. Closing files happens Lately in the program (not when the reading client logs. Faster)
// 3. Ensure 1 owndership of a File descriptors
class UniqueFD
{
    int fd;

  public:
    // Constructor. Default is -1 (invalid FD)
    explicit UniqueFD(int f = -1) : fd(f)
    {
    }

    // Prevent copying (don't want two objects closing the same FD)
    UniqueFD(const UniqueFD &) = delete;
    UniqueFD &operator=(const UniqueFD &) = delete;

    // Allow moving (transfer ownership of the FD)
    // NOTE: Double && means that other is an rvalue reference
    // meaning it can bind to a temporary object that is about to be destroyed
    // only one object will own the FD at a time
    UniqueFD(UniqueFD &&other) noexcept : fd(other.release())
    {
    }
    UniqueFD &operator=(UniqueFD &&other) noexcept
    {
        reset(other.release());
        return *this;
    }

    // Destructor
    ~UniqueFD()
    {
        reset();
    }

    // setter but makes sure to close the old FD if valid
    void reset(int new_fd = -1)
    {
        if (fd != -1)
            close(fd);
        fd = new_fd;
    }

    // Getter. No need for a setter since we have reset()
    int get() const
    {
        return fd;
    }

    // Check if the FD is valid
    bool is_valid() const
    {
        return fd != -1;
    }

  private:
    // No need to close the file descriptor because destruction or reset will handle it
    int release()
    {
        int temp = fd;
        fd = -1;
        return temp;
    }
};

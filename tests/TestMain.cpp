#include "TestSupport.h"
namespace test
{
int runWorkerAllocationProbe();
}
int main(int argc, char **argv)
{
    if (argc > 1 && std::string(argv[1]) == "--worker-allocation-probe")
        return test::runWorkerAllocationProbe();
    int failed = 0;
    for (const auto &t : test::cases())
    {
        try
        {
            t.run();
            std::cout << "PASS " << t.name << '\n';
        }
        catch (const std::exception &e)
        {
            ++failed;
            std::cerr << "FAIL " << t.name << ": " << e.what() << '\n';
        }
    }
    std::cout << test::cases().size() - failed << '/' << test::cases().size() << " passed\n";
    return failed ? 1 : 0;
}

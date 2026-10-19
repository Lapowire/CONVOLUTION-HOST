#include "TestSupport.h"
int main() {
 int failed=0;
 for (const auto& t: test::cases()) {
  try { t.run(); std::cout << "PASS " << t.name << '\n'; }
  catch(const std::exception& e) { ++failed; std::cerr << "FAIL " << t.name << ": " << e.what() << '\n'; }
 }
 std::cout << test::cases().size()-failed << '/' << test::cases().size() << " passed\n";
 return failed ? 1 : 0;
}

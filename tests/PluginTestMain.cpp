#include "TestSupport.h"
#include <juce_gui_basics/juce_gui_basics.h>
int main() {
 juce::ScopedJuceInitialiser_GUI gui;int failed=0;
 for(const auto& t:test::cases()) {
  try{t.run();std::cout<<"PASS "<<t.name<<std::endl;}
  catch(const std::exception& e){++failed;std::cerr<<"FAIL "<<t.name<<": "<<e.what()<<std::endl;}
 }
 std::cout<<test::cases().size()-failed<<'/'<<test::cases().size()<<" passed\n";return failed?1:0;
}
